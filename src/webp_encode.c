#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "webp_encode.h"
#include "win_encode.h"

int webp_picture_writer(const uint8_t* data, size_t data_size, const WebPPicture* picture)
{
	if (data_size > 0) {
		KREQUEST r = picture->user_data;
		kgl_async_context *async_ctx = (kgl_async_context *)picture->custom_ptr;
		webp_context *ctx = (webp_context *)async_ctx->module;
		KGL_RESULT result = async_ctx->out->f->write_body(async_ctx->out, r, (const char *)data, (int)data_size);
		return result == KGL_OK;
	}
	return 1;
}
bool webp_read_picture(webp_context *webp) {
	webp->is_gif = false;
	assert(webp->u.data==NULL);
	int ok;
	webp->u.picture = (struct WebPPicture*)malloc(sizeof(struct WebPPicture));
	memset(webp->u.picture,0,sizeof(struct WebPPicture));
	if (!WebPPictureInit(webp->u.picture)) {
		return false;
	}
#ifdef _WIN32
	ok = WindowsReadPicture(webp->buff.data, webp->u.picture);
#else
	ok = unix_read_picture(webp->buff.data, webp->buff.used, webp->u.picture);
	if (!ok) {
		WebPPictureFree(webp->u.picture);
		free(webp->u.picture);
#ifdef WEBP_HAVE_GIF
		webp->is_gif = true;
		webp->u.gif_data = (WebPData *)malloc(sizeof(WebPData));
		WebPDataInit(webp->u.gif_data);
		if (KGL_OK == gif2webp(webp,webp->u.gif_data)) {
			ok = true;
		}
#endif
	}
#endif
	//printf("webp_read_picture result=[%d]\n",ok);
	return ok;
}
KGL_RESULT webp_encode_picture(KREQUEST rq, kgl_async_context *async_ctx) {
	webp_context *ctx = (webp_context *)async_ctx->module;
#ifdef _WIN32
	assert(!ctx->is_gif);
#endif
	if (!ctx->is_gif) {
		ctx->u.picture->user_data = rq;
		ctx->u.picture->custom_ptr = async_ctx;
		ctx->u.picture->writer = webp_picture_writer;
		if (!WebPEncode(&ctx->config, ctx->u.picture)) {
			fprintf(stderr, "Error! Cannot encode picture as WebP\n");
			fprintf(stderr, "Error code: %d\n",ctx->u.picture->error_code);
			return KGL_EDATA_FORMAT;
		}
		return KGL_OK;
	}
	return async_ctx->out->f->write_body(async_ctx->out,rq,(char *)ctx->u.gif_data->bytes, (int)ctx->u.gif_data->size);
}
char *strlendup(const char *str, int len)
{
	char *buf = (char *)malloc(len + 1);
	memcpy(buf, str, len);
	buf[len] = '\0';
	return buf;
}
