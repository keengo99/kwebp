// Copyright 2013 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Windows Imaging Component (WIC) decode.

//#include "./wicdec.h"

#ifdef HAVE_CONFIG_H
#include "webp/config.h"
#endif
#ifdef _WIN32
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef __MINGW32__
#define INITGUID  // Without this GUIDs are declared extern and fail to link
#endif
#define CINTERFACE
#define COBJMACROS
#define _WIN32_IE 0x500  // Workaround bug in shlwapi.h when compiling C++
						 // code with COBJMACROS.
#include <ole2.h>  // CreateStreamOnHGlobal()
#include <shlwapi.h>
#include <tchar.h>
#include <windows.h>
#include <wincodec.h>

#include "webp/encode.h"

#define IFS(fn)                                                     \
  do {                                                              \
    if (SUCCEEDED(hr)) {                                            \
      hr = (fn);                                                    \
      if (FAILED(hr)) fprintf(stderr, #fn " failed %08lx\n", hr);   \
    }                                                               \
  } while (0)

// modified version of DEFINE_GUID from guiddef.h.
#define WEBP_DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
  static const GUID name = \
      { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }

#ifdef __cplusplus
#define MAKE_REFGUID(x) (x)
#else
#define MAKE_REFGUID(x) &(x)
#endif

typedef struct WICFormatImporter {
	const GUID* pixel_format;
	int bytes_per_pixel;
	int(*import)(WebPPicture* const, const uint8_t* const, int);
} WICFormatImporter;

// From Microsoft SDK 7.0a -- wincodec.h
// Create local copies for compatibility when building against earlier
// versions of the SDK.
WEBP_DEFINE_GUID(GUID_WICPixelFormat24bppBGR_,
	0x6fddc324, 0x4e03, 0x4bfe,
	0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0c);
WEBP_DEFINE_GUID(GUID_WICPixelFormat24bppRGB_,
	0x6fddc324, 0x4e03, 0x4bfe,
	0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0d);
WEBP_DEFINE_GUID(GUID_WICPixelFormat32bppBGRA_,
	0x6fddc324, 0x4e03, 0x4bfe,
	0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0f);
WEBP_DEFINE_GUID(GUID_WICPixelFormat32bppRGBA_,
	0xf5c7ad2d, 0x6a8d, 0x43dd,
	0xa7, 0xa8, 0xa2, 0x99, 0x35, 0x26, 0x1a, 0xe9);
WEBP_DEFINE_GUID(GUID_WICPixelFormat64bppBGRA_,
	0x1562ff7c, 0xd352, 0x46f9,
	0x97, 0x9e, 0x42, 0x97, 0x6b, 0x79, 0x22, 0x46);
WEBP_DEFINE_GUID(GUID_WICPixelFormat64bppRGBA_,
	0x6fddc324, 0x4e03, 0x4bfe,
	0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x16);
int ImgIoUtilCheckSizeArgumentsOverflow(uint64_t nmemb, size_t size) {
	const uint64_t total_size = nmemb * size;
	int ok = (total_size == (size_t)total_size);
#if defined(WEBP_MAX_IMAGE_SIZE)
	ok = ok && (total_size <= (uint64_t)WEBP_MAX_IMAGE_SIZE);
#endif
	return ok;
}
static HRESULT CreateInputStream(HGLOBAL data, IStream** stream) {
	HRESULT hr = S_OK;
	IFS(CreateStreamOnHGlobal(data, FALSE, stream));
	return hr;
}
static int HasPalette(GUID pixel_format) {
	return (IsEqualGUID(MAKE_REFGUID(pixel_format),
		MAKE_REFGUID(GUID_WICPixelFormat1bppIndexed)) ||
		IsEqualGUID(MAKE_REFGUID(pixel_format),
			MAKE_REFGUID(GUID_WICPixelFormat2bppIndexed)) ||
		IsEqualGUID(MAKE_REFGUID(pixel_format),
			MAKE_REFGUID(GUID_WICPixelFormat4bppIndexed)) ||
		IsEqualGUID(MAKE_REFGUID(pixel_format),
			MAKE_REFGUID(GUID_WICPixelFormat8bppIndexed)));
}

static int HasAlpha(IWICImagingFactory* const factory,
	IWICBitmapDecoder* const decoder,
	IWICBitmapFrameDecode* const frame,
	GUID pixel_format) {
	int has_alpha;
	if (HasPalette(pixel_format)) {
		IWICPalette* frame_palette = NULL;
		IWICPalette* global_palette = NULL;
		BOOL frame_palette_has_alpha = FALSE;
		BOOL global_palette_has_alpha = FALSE;

		// A palette may exist at the frame or container level,
		// check IWICPalette::HasAlpha() for both if present.
		if (SUCCEEDED(IWICImagingFactory_CreatePalette(factory, &frame_palette)) &&
			SUCCEEDED(IWICBitmapFrameDecode_CopyPalette(frame, frame_palette))) {
			IWICPalette_HasAlpha(frame_palette, &frame_palette_has_alpha);
		}
		if (SUCCEEDED(IWICImagingFactory_CreatePalette(factory, &global_palette)) &&
			SUCCEEDED(IWICBitmapDecoder_CopyPalette(decoder, global_palette))) {
			IWICPalette_HasAlpha(global_palette, &global_palette_has_alpha);
		}
		has_alpha = frame_palette_has_alpha || global_palette_has_alpha;

		if (frame_palette != NULL) IUnknown_Release(frame_palette);
		if (global_palette != NULL) IUnknown_Release(global_palette);
	} else {
		has_alpha = IsEqualGUID(MAKE_REFGUID(pixel_format),
			MAKE_REFGUID(GUID_WICPixelFormat32bppRGBA_)) ||
			IsEqualGUID(MAKE_REFGUID(pixel_format),
				MAKE_REFGUID(GUID_WICPixelFormat32bppBGRA_)) ||
			IsEqualGUID(MAKE_REFGUID(pixel_format),
				MAKE_REFGUID(GUID_WICPixelFormat64bppRGBA_)) ||
			IsEqualGUID(MAKE_REFGUID(pixel_format),
				MAKE_REFGUID(GUID_WICPixelFormat64bppBGRA_));
	}
	return has_alpha;
}
int WindowsReadPicture(HGLOBAL data,WebPPicture* const pic) {
	// From Microsoft SDK 6.0a -- ks.h
	// Define a local copy to avoid link errors under mingw.
	WEBP_DEFINE_GUID(GUID_NULL_, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	static const WICFormatImporter kAlphaFormatImporters[] = {
	  { &GUID_WICPixelFormat32bppBGRA_, 4, WebPPictureImportBGRA },
	  { &GUID_WICPixelFormat32bppRGBA_, 4, WebPPictureImportRGBA },
	  { NULL, 0, NULL },
	};
	static const WICFormatImporter kNonAlphaFormatImporters[] = {
	  { &GUID_WICPixelFormat24bppBGR_, 3, WebPPictureImportBGR },
	  { &GUID_WICPixelFormat24bppRGB_, 3, WebPPictureImportRGB },
	  { NULL, 0, NULL },
	};
	HRESULT hr = S_OK;
	IWICBitmapFrameDecode* frame = NULL;
	IWICFormatConverter* converter = NULL;
	IWICImagingFactory* factory = NULL;
	IWICBitmapDecoder* decoder = NULL;
	IStream* stream = NULL;
	UINT frame_count = 0;
	UINT width = 0, height = 0;
	BYTE* rgb = NULL;
	WICPixelFormatGUID src_pixel_format = GUID_WICPixelFormatUndefined;
	const WICFormatImporter* importer = NULL;
	GUID src_container_format = GUID_NULL_;
	static const GUID* kAlphaContainers[] = {
	  &GUID_ContainerFormatBmp,
	  &GUID_ContainerFormatPng,
	  &GUID_ContainerFormatTiff,
	  NULL
	};
	int has_alpha = 0;
	int64_t stride;

	if (data == NULL || pic == NULL) return 0;

	IFS(CoInitialize(NULL));
	IFS(CoCreateInstance(MAKE_REFGUID(CLSID_WICImagingFactory), NULL,
		CLSCTX_INPROC_SERVER,
		MAKE_REFGUID(IID_IWICImagingFactory),
		(LPVOID*)&factory));
	if (hr == REGDB_E_CLASSNOTREG) {
		fprintf(stderr,
			"Couldn't access Windows Imaging Component (are you running "
			"Windows XP SP3 or newer?). Most formats not available. "
			"Use -s for the available YUV input.\n");
	}
	// Prepare for image decoding.
	IFS(CreateInputStream(data, &stream));

	IFS(IWICImagingFactory_CreateDecoderFromStream(
		factory, stream, NULL,
		WICDecodeMetadataCacheOnDemand, &decoder));
	IFS(IWICBitmapDecoder_GetFrameCount(decoder, &frame_count));
	if (SUCCEEDED(hr)) {
		if (frame_count != 1) {
			fprintf(stderr, "frame count [%d] error\n",frame_count);
			hr = E_FAIL;
		}
	}
	IFS(IWICBitmapDecoder_GetFrame(decoder, 0, &frame));
	IFS(IWICBitmapFrameDecode_GetPixelFormat(frame, &src_pixel_format));
	IFS(IWICBitmapDecoder_GetContainerFormat(decoder, &src_container_format));
	if (SUCCEEDED(hr)) {
		const GUID** guid;
		for (guid = kAlphaContainers; *guid != NULL; ++guid) {
			if (IsEqualGUID(MAKE_REFGUID(src_container_format),
				MAKE_REFGUID(**guid))) {
				has_alpha = HasAlpha(factory, decoder, frame, src_pixel_format);
				break;
			}
		}
	}
	// Prepare for pixel format conversion (if necessary).
	IFS(IWICImagingFactory_CreateFormatConverter(factory, &converter));

	for (importer = has_alpha ? kAlphaFormatImporters : kNonAlphaFormatImporters;
		hr == S_OK && importer->import != NULL; ++importer) {
		BOOL can_convert;
		const HRESULT cchr = IWICFormatConverter_CanConvert(
			converter,
			MAKE_REFGUID(src_pixel_format),
			MAKE_REFGUID(*importer->pixel_format),
			&can_convert);
		if (SUCCEEDED(cchr) && can_convert) break;
	}
	if (importer->import == NULL) hr = E_FAIL;

	IFS(IWICFormatConverter_Initialize(converter, (IWICBitmapSource*)frame,
		importer->pixel_format,
		WICBitmapDitherTypeNone,
		NULL, 0.0, WICBitmapPaletteTypeCustom));

	// Decode.
	IFS(IWICFormatConverter_GetSize(converter, &width, &height));
	stride = (int64_t)importer->bytes_per_pixel * width * sizeof(*rgb);
	if (stride != (int)stride ||
		!ImgIoUtilCheckSizeArgumentsOverflow(stride, height)) {
		hr = E_FAIL;
	}

	if (SUCCEEDED(hr)) {
		rgb = (BYTE*)malloc((size_t)stride * height);
		if (rgb == NULL)
			hr = E_OUTOFMEMORY;
	}
	IFS(IWICFormatConverter_CopyPixels(converter, NULL,
		(UINT)stride, (UINT)stride * height, rgb));

	// WebP conversion.
	if (SUCCEEDED(hr)) {
		int ok;
		pic->width = width;
		pic->height = height;
		pic->use_argb = 1;    // For WIC, we always force to argb
		ok = importer->import(pic, rgb, (int)stride);
		if (!ok) hr = E_FAIL;
	}
#if 0
	if (SUCCEEDED(hr)) {
		if (metadata != NULL) {
			hr = ExtractMetadata(factory, frame, metadata);
			if (FAILED(hr)) {
				fprintf(stderr, "Error extracting image metadata using WIC!\n");
			}
		}
	}
#endif
	// Cleanup.
	if (converter != NULL) IUnknown_Release(converter);
	if (frame != NULL) IUnknown_Release(frame);
	if (decoder != NULL) IUnknown_Release(decoder);
	if (factory != NULL) IUnknown_Release(factory);
	if (stream != NULL) IUnknown_Release(stream);
	free(rgb);
	return SUCCEEDED(hr);
}
#endif

