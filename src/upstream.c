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
	kgl_output_stream out;
	init_push_gate(ctx, &out);
	assert(!c->is_webp);
	return ctx->f->open_next(rq, ctx->cn, ctx->in, &out, NULL);
}
static kgl_upstream upstream = {
	sizeof(kgl_upstream),
	0,
	"webp",
	create_ctx,
	free_ctx,
	webp_open,
	NULL
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
