#include <assert.h>
#include <string.h>
#include "upstream.h"
#include "ksapi.h"
#include "webp_encode.h"

static void * create_ctx()
{
	return NULL;
}
static void free_webp_context(webp_context *c)
{
	buffer_destroy(&c->buff);
	if (c->u.data) {
		if (c->is_gif) {
			WebPDataClear(c->u.gif_data);
		} else {
			WebPPictureFree(c->u.picture);
		}
		free(c->u.data);
		c->u.data = NULL;
	}
	if (c->vary) {
		free(c->vary);
		c->vary = NULL;
	}
	if (c->origin_content_type) {
		free(c->origin_content_type);
		c->origin_content_type = NULL;
	}
}
static void free_ctx(void *ctx)
{
	webp_context *c = (webp_context *)ctx;
	free_webp_context(c);
	free(c);
}
static KGL_RESULT webp_open(KREQUEST rq, kgl_async_context *ctx)
{
	webp_context *c = (webp_context *)ctx->module;
	kgl_output_stream *out = init_push_gate(ctx);
	assert(!c->is_webp);
	KGL_RESULT result = ctx->f->open_next(rq, ctx->cn, ctx->in, out, NULL);
	free_webp_context(c);
	out->f->release(out);
	return result;
}
#if 0
static KGL_RESULT webp_read(KREQUEST rq, kgl_async_context *ctx)
{
	webp_context *webp = (webp_context *)ctx->module;
	if (!webp->upstream_was_body_finish) {
		if (ctx->us) {
			return ctx->us->read(rq, ctx);
		}
		return ctx->gate->f->push_message(ctx->gate, rq, KGL_MSG_ERROR, 500, "no upstream");
	}
	if (webp->no_encode) {
		if (webp->buff.data == NULL) {
			return ctx->gate->f->push_body_finish(ctx->gate, rq, webp->upstream_push_body_result);
		}
#ifdef _WIN32
		char *image_mem = (char *)GlobalLock(webp->buff.data);
		KGL_RESULT result = ctx->gate->f->push_body(ctx->gate, rq, image_mem, webp->buff.used);
		GlobalUnlock(image_mem);
#else
		KGL_RESULT result = ctx->gate->f->push_body(ctx->gate, rq, webp->buff.data, webp->buff.used);
#endif
		if (result == KGL_OK) {
			result = webp->upstream_push_body_result;
		}
		return ctx->gate->f->push_body_finish(ctx->gate, rq, result);
	}
	return ctx->gate->f->push_body_finish(ctx->gate, rq, webp_encode_picture(rq, ctx));
}

static void webp_close(KREQUEST rq, kgl_async_context *ctx)
{
	webp_context *c = (webp_context *)ctx->module;
	free_webp_context(c);
	if (ctx->us) {
		ctx->us->close(rq, ctx);
	}
}
#endif
static kgl_async_upstream upstream = {
	sizeof(kgl_async_upstream),
	0,
	"webp",
	create_ctx,
	free_ctx,
	NULL,
	webp_open
};
bool register_upstream(KREQUEST rq, kgl_access_context *ctx, webp_context *c)
{
	return KGL_OK == ctx->f->support_function(rq, ctx->cn, KF_REQ_UPSTREAM, &upstream, (void **)&c);
}
bool init_webp_context(webp_context *ctx, WebPConfig *config)
{
	memset(ctx, 0, sizeof(webp_context));
	memcpy(&ctx->config, config, sizeof(WebPConfig));
	return true;
}
