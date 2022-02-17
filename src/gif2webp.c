/*
 * gif2webp.c
 *
 *  Created on: Jun 14, 2020
 *      Author: keengo
 */
#include <string.h>
#include <assert.h>
#include "gifdec.h"
#include "webp/encode.h"
#include "webp/mux.h"
#include "imageio/imageio_util.h"
#include "upstream.h"
#ifdef WEBP_HAVE_GIF
#define MAX_GIF_DATA 8388608
static int transparent_index = GIF_INDEX_INVALID;  // Opaque by default.

static const char* const kErrorMessages[-WEBP_MUX_NOT_ENOUGH_DATA + 1] = {
  "WEBP_MUX_NOT_FOUND", "WEBP_MUX_INVALID_ARGUMENT", "WEBP_MUX_BAD_DATA",
  "WEBP_MUX_MEMORY_ERROR", "WEBP_MUX_NOT_ENOUGH_DATA"
};

static const char* ErrorString(WebPMuxError err) {
  assert(err <= WEBP_MUX_NOT_FOUND && err >= WEBP_MUX_NOT_ENOUGH_DATA);
  return kErrorMessages[-err];
}

enum {
  METADATA_ICC  = (1 << 0),
  METADATA_XMP  = (1 << 1),
  METADATA_ALL  = METADATA_ICC | METADATA_XMP
};

static int webp_gif_read(GifFileType *gif, GifByteType *buf, int len)
{
	webp_buffer *buff = (webp_buffer *)gif->UserData;
	len = MIN(len,buff->used);
	memcpy(buf,buff->data,len);
	buff->used -= len;
	buff->data += len;
	return len;
}
KGL_RESULT gif2webp(webp_context *webp,WebPData *webp_data) {
	//printf("gif2webp...........\n");
	  //switch buffer to read
	webp_buffer buff;
	buff.data = webp->buff.data;
	buff.used = webp->buff.used;
	  int gif_error = GIF_ERROR;
	KGL_RESULT result = KGL_EUNKNOW;
	  GifFileType *gif = DGifOpen(&buff ,webp_gif_read, &gif_error);
	  if (gif==NULL) {
		printf("cann't open gif to read\n");
		if (gif_error != GIF_OK) {
		     GIFDisplayError(gif, gif_error);
	   	}
		return KGL_EUNKNOW;
	  }

	  int verbose = 0;
	  WebPMuxError err = WEBP_MUX_OK;

	  int frame_duration = 0;
	  int frame_timestamp = 0;
	  GIFDisposeMethod orig_dispose = GIF_DISPOSE_NONE;

	  WebPPicture frame;                // Frame rectangle only (not disposed).
	  WebPPicture curr_canvas;          // Not disposed.
	  WebPPicture prev_canvas;          // Disposed.

	  WebPAnimEncoder* enc = NULL;
	  WebPAnimEncoderOptions enc_options;
	  //WebPConfig config;

	  int frame_number = 0;     // Whether we are processing the first frame.
	  int done;


	  int keep_metadata = METADATA_XMP;  // ICC not output by default.
	  WebPData icc_data;
	  int stored_icc = 0;         // Whether we have already stored an ICC profile.
	  WebPData xmp_data;
	  int stored_xmp = 0;         // Whether we have already stored an XMP profile.
	  int loop_count = 0;         // default: infinite
	  int stored_loop_count = 0;  // Whether we have found an explicit loop count.
	  int loop_compatibility = 0;
	  WebPMux* mux = NULL;


	  if (!WebPAnimEncoderOptionsInit(&enc_options) ||
	       !WebPPictureInit(&frame) || !WebPPictureInit(&curr_canvas) ||
	       !WebPPictureInit(&prev_canvas)) {
		fprintf(stderr, "Error! Version mismatch!\n");
		return result;
	   }


	   WebPDataInit(&icc_data);
	   WebPDataInit(&xmp_data);
	  // Loop over GIF images
	   done = 0;
	   do {
	     GifRecordType type;
	     if (DGifGetRecordType(gif, &type) == GIF_ERROR) goto End;
	     switch (type) {
	         case IMAGE_DESC_RECORD_TYPE: {
	        	 GIFFrameRect gif_rect;
	        	        GifImageDesc* const image_desc = &gif->Image;

	        	        if (!DGifGetImageDesc(gif)) goto End;

	        	        if (frame_number == 0) {
	        	          if (verbose) {
	        	            printf("Canvas screen: %d x %d\n", gif->SWidth, gif->SHeight);
	        	          }
	        	          // Fix some broken GIF global headers that report
	        	          // 0 x 0 screen dimension.
	        	          if (gif->SWidth == 0 || gif->SHeight == 0) {
	        	            image_desc->Left = 0;
	        	            image_desc->Top = 0;
	        	            gif->SWidth = image_desc->Width;
	        	            gif->SHeight = image_desc->Height;
	        	            if (gif->SWidth <= 0 || gif->SHeight <= 0) {
	        	              goto End;
	        	            }
	        	            if (verbose) {
	        	              printf("Fixed canvas screen dimension to: %d x %d\n",
	        	                     gif->SWidth, gif->SHeight);
	        	            }
	        	          }
	        	          // Allocate current buffer.
	        	          frame.width = gif->SWidth;
	        	          frame.height = gif->SHeight;
	        	          frame.use_argb = 1;
	        	          if (!WebPPictureAlloc(&frame)) goto End;
	        	          GIFClearPic(&frame, NULL);
	        	          WebPPictureCopy(&frame, &curr_canvas);
	        	          WebPPictureCopy(&frame, &prev_canvas);

	        	          // Background color.
	        	          GIFGetBackgroundColor(gif->SColorMap, gif->SBackGroundColor,
	        	                                transparent_index,
	        	                                &enc_options.anim_params.bgcolor);

	        	          // Initialize encoder.
	        	          enc = WebPAnimEncoderNew(curr_canvas.width, curr_canvas.height,
	        	                                   &enc_options);
	        	          if (enc == NULL) {
	        	            fprintf(stderr,
	        	                    "Error! Could not create encoder object. Possibly due to "
	        	                    "a memory error.\n");
	        	            goto End;
	        	          }
	        	        }

	        	        // Some even more broken GIF can have sub-rect with zero width/height.
	        	        if (image_desc->Width == 0 || image_desc->Height == 0) {
	        	          image_desc->Width = gif->SWidth;
	        	          image_desc->Height = gif->SHeight;
	        	        }

	        	        if (!GIFReadFrame(gif, transparent_index, &gif_rect, &frame)) {
	        	          goto End;
	        	        }
	        	        // Blend frame rectangle with previous canvas to compose full canvas.
	        	        // Note that 'curr_canvas' is same as 'prev_canvas' at this point.
	        	        GIFBlendFrames(&frame, &gif_rect, &curr_canvas);

	        	        if (!WebPAnimEncoderAdd(enc, &curr_canvas, frame_timestamp, &webp->config)) {
	        	          fprintf(stderr, "Error while adding frame #%d: %s\n", frame_number,
	        	                  WebPAnimEncoderGetError(enc));
	        	          goto End;
	        	        } else {
	        	          ++frame_number;
	        	        }

	        	        // Update canvases.
	        	        GIFDisposeFrame(orig_dispose, &gif_rect, &prev_canvas, &curr_canvas);
	        	        GIFCopyPixels(&curr_canvas, &prev_canvas);

	        	        // Force frames with a small or no duration to 100ms to be consistent
	        	        // with web browsers and other transcoding tools. This also avoids
	        	        // incorrect durations between frames when padding frames are
	        	        // discarded.
	        	        if (frame_duration <= 10) {
	        	          frame_duration = 100;
	        	        }

	        	        // Update timestamp (for next frame).
	        	        frame_timestamp += frame_duration;

	        	        // In GIF, graphic control extensions are optional for a frame, so we
	        	        // may not get one before reading the next frame. To handle this case,
	        	        // we reset frame properties to reasonable defaults for the next frame.
	        	        orig_dispose = GIF_DISPOSE_NONE;
	        	        frame_duration = 0;
	        	        transparent_index = GIF_INDEX_INVALID;
				if (frame_number * gif->SWidth * gif->SHeight > MAX_GIF_DATA) {
					fprintf(stderr,"gif is too big\n");
					goto End;
				}
	        	        break;
	         }
	         case EXTENSION_RECORD_TYPE: {
	                 int extension;
	                 GifByteType* data = NULL;
	                 if (DGifGetExtension(gif, &extension, &data) == GIF_ERROR) {
	                   goto End;
	                 }
	                 if (data == NULL) continue;

	                 switch (extension) {
	                   case COMMENT_EXT_FUNC_CODE: {
	                     break;  // Do nothing for now.
	                   }
	                   case GRAPHICS_EXT_FUNC_CODE: {
	                     if (!GIFReadGraphicsExtension(data, &frame_duration, &orig_dispose,
	                                                   &transparent_index)) {
	                       goto End;
	                     }
	                     break;
	                   }
	                   case PLAINTEXT_EXT_FUNC_CODE: {
	                     break;
	                   }
	                   case APPLICATION_EXT_FUNC_CODE: {
	                     if (data[0] != 11) break;    // Chunk is too short
	                     if (!memcmp(data + 1, "NETSCAPE2.0", 11) ||
	                         !memcmp(data + 1, "ANIMEXTS1.0", 11)) {
	                       if (!GIFReadLoopCount(gif, &data, &loop_count)) {
	                         goto End;
	                       }
	                       if (verbose) {
	                         fprintf(stderr, "Loop count: %d\n", loop_count);
	                       }
	                       stored_loop_count = loop_compatibility ? (loop_count != 0) : 1;
	                     } else {  // An extension containing metadata.
	                       // We only store the first encountered chunk of each type, and
	                       // only if requested by the user.
	                       const int is_xmp = (keep_metadata & METADATA_XMP) &&
	                                          !stored_xmp &&
	                                          !memcmp(data + 1, "XMP DataXMP", 11);
	                       const int is_icc = (keep_metadata & METADATA_ICC) &&
	                                          !stored_icc &&
	                                          !memcmp(data + 1, "ICCRGBG1012", 11);
	                       if (is_xmp || is_icc) {
	                         if (!GIFReadMetadata(gif, &data,
	                                              is_xmp ? &xmp_data : &icc_data)) {
	                           goto End;
	                         }
	                         if (is_icc) {
	                           stored_icc = 1;
	                         } else if (is_xmp) {
	                           stored_xmp = 1;
	                         }
	                       }
	                     }
	                     break;
	                   }
	                   default: {
	                     break;  // skip
	                   }
	                 }
	                 while (data != NULL) {
	                   if (DGifGetExtensionNext(gif, &data) == GIF_ERROR) goto End;
	                 }
	                 break;
	               }
	         case TERMINATE_RECORD_TYPE: {
				done = 1;
				break;
			  }
			  default: {
				fprintf(stderr, "Skipping over unknown record type %d\n", type);
				break;
			  }
	     }
	   } while (!done);

	   // Last NULL frame.
	   if (!WebPAnimEncoderAdd(enc, NULL, frame_timestamp, NULL)) {
	     fprintf(stderr, "Error flushing WebP muxer.\n");
	     fprintf(stderr, "%s\n", WebPAnimEncoderGetError(enc));
	   }

	   if (!WebPAnimEncoderAssemble(enc, webp_data)) {
	     fprintf(stderr, "%s\n", WebPAnimEncoderGetError(enc));
	     goto End;
	   }

	   if (!loop_compatibility) {
	     if (!stored_loop_count) {
	       // if no loop-count element is seen, the default is '1' (loop-once)
	       // and we need to signal it explicitly in WebP. Note however that
	       // in case there's a single frame, we still don't need to store it.
	       if (frame_number > 1) {
	         stored_loop_count = 1;
	         loop_count = 1;
	       }
	     } else if (loop_count > 0 && loop_count < 65535) {
	       // adapt GIF's semantic to WebP's (except in the infinite-loop case)
	       loop_count += 1;
	     }
	   }
	   // loop_count of 0 is the default (infinite), so no need to signal it
	   if (loop_count == 0) stored_loop_count = 0;

	   if (stored_loop_count || stored_icc || stored_xmp) {
	     // Re-mux to add loop count and/or metadata as needed.
	     mux = WebPMuxCreate(webp_data, 1);
	     if (mux == NULL) {
	       fprintf(stderr, "ERROR: Could not re-mux to add loop count/metadata.\n");
	       goto End;
	     }
	     WebPDataClear(webp_data);

	     if (stored_loop_count) {  // Update loop count.
	       WebPMuxAnimParams new_params;
	       err = WebPMuxGetAnimationParams(mux, &new_params);
	       if (err != WEBP_MUX_OK) {
	         fprintf(stderr, "ERROR (%s): Could not fetch loop count.\n",
	                 ErrorString(err));
	         goto End;
	       }
	       new_params.loop_count = loop_count;
	       err = WebPMuxSetAnimationParams(mux, &new_params);
	       if (err != WEBP_MUX_OK) {
	         fprintf(stderr, "ERROR (%s): Could not update loop count.\n",
	                 ErrorString(err));
	         goto End;
	       }
	     }

	     if (stored_icc) {   // Add ICCP chunk.
	       err = WebPMuxSetChunk(mux, "ICCP", &icc_data, 1);
	       if (verbose) {
	         fprintf(stderr, "ICC size: %d\n", (int)icc_data.size);
	       }
	       if (err != WEBP_MUX_OK) {
	         fprintf(stderr, "ERROR (%s): Could not set ICC chunk.\n",
	                 ErrorString(err));
	         goto End;
	       }
	     }

	     if (stored_xmp) {   // Add XMP chunk.
	       err = WebPMuxSetChunk(mux, "XMP ", &xmp_data, 1);
	       if (verbose) {
	         fprintf(stderr, "XMP size: %d\n", (int)xmp_data.size);
	       }
	       if (err != WEBP_MUX_OK) {
	         fprintf(stderr, "ERROR (%s): Could not set XMP chunk.\n",
	                 ErrorString(err));
	         goto End;
	       }
	     }

	     err = WebPMuxAssemble(mux, webp_data);
	     if (err != WEBP_MUX_OK) {
	       fprintf(stderr, "ERROR (%s): Could not assemble when re-muxing to add "
	               "loop count/metadata.\n", ErrorString(err));
	       goto End;
	     }
	   }
	   // All OK.
	   result = KGL_OK;
	   gif_error = GIF_OK;

	  End:
	   WebPDataClear(&icc_data);
	   WebPDataClear(&xmp_data);
	   WebPMuxDelete(mux);
	   WebPPictureFree(&frame);
	   WebPPictureFree(&curr_canvas);
	   WebPPictureFree(&prev_canvas);
	   WebPAnimEncoderDelete(enc);

	   if (gif_error != GIF_OK) {
	     GIFDisplayError(gif, gif_error);
	   }
	   if (gif != NULL) {
#if LOCAL_GIF_PREREQ(5,1)
	     DGifCloseFile(gif, &gif_error);
#else
	     DGifCloseFile(gif);
#endif
	   }
	   return result;
}
#endif
