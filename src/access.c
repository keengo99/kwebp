#include <string.h>
#include <stdio.h>
#include "ksapi.h"
#include "mark.h"
#include "upstream.h"
#include "vary.h"
#define MAX_WEBP_SIZE 16777216
static void *create_ctx()
{
	webp_mark *m = (webp_mark *)malloc(sizeof(webp_mark));
	memset(m, 0, sizeof(webp_mark));
	m->quality = 75;
	if (!WebPConfigPreset(&m->config, WEBP_PRESET_DEFAULT, (float)m->quality)) {
		fprintf(stderr, "cann't init config\n");
	}
	return m;
}
static void free_ctx(void *ctx)
{
	webp_mark *m = (webp_mark *)ctx;
	free(m);
}
static KGL_RESULT build(kgl_access_build *build_ctx, KF_ACCESS_BUILD_TYPE build_type)
{
	webp_mark *m = (webp_mark *)build_ctx->module;
	char buf[512];
	int len = sprintf(buf, "%d", m->quality);
	switch (build_type) {
	case KF_ACCESS_BUILD_SHORT:
		build_ctx->write_string(build_ctx->cn, buf, len, 0);
		break;
	case KF_ACCESS_BUILD_HTML:
		build_ctx->write_string(build_ctx->cn, kgl_expand_string("quality:<input name='quality' value='"), 0);
		build_ctx->write_string(build_ctx->cn, buf, len, 0);
		build_ctx->write_string(build_ctx->cn, kgl_expand_string("'/>1 - 100(best)<br>"), 0);
		build_ctx->write_string(build_ctx->cn, kgl_expand_string("max:<input name='max' value='"), 0);
		len = sprintf(buf, "%d", m->max_length);
		build_ctx->write_string(build_ctx->cn, buf, len, 0);
		build_ctx->write_string(build_ctx->cn, kgl_expand_string("'/>(default:16M)"), 0);
		break;
	case KF_ACCESS_BUILD_XML:
		build_ctx->write_string(build_ctx->cn, kgl_expand_string("quality='"), 0);
		build_ctx->write_string(build_ctx->cn, buf, len, 0);
		build_ctx->write_string(build_ctx->cn, kgl_expand_string("' max='"), 0);
		len = sprintf(buf, "%d", m->max_length);
		build_ctx->write_string(build_ctx->cn, buf, len, 0);
		build_ctx->write_string(build_ctx->cn, kgl_expand_string("'"), 0);
		break;
	}
	return KGL_OK;
}
static KGL_RESULT parse(kgl_access_parse *parse_ctx, KF_ACCESS_PARSE_TYPE parse_type)
{
	webp_mark *m = (webp_mark *)parse_ctx->module;
	switch (parse_type) {
	case KF_ACCESS_PARSE_KV:
	{
		const char *quality = parse_ctx->get_value(parse_ctx->cn, "quality");
		if (quality) {
			int v = atoi(quality);
			if (WebPConfigPreset(&m->config, WEBP_PRESET_DEFAULT, (float)v)) {
				m->quality = v;
			}
		}
		const char *max_length = parse_ctx->get_value(parse_ctx->cn, "max");
		if (max_length) {
			m->max_length = atoi(max_length);
		}
		break;
	}
	default:
		break;
	}
	return KGL_OK;
}
static uint32_t process(KREQUEST rq, kgl_access_context *ctx, DWORD notify)
{
	webp_mark *m = (webp_mark *)ctx->module;
	char buf[512];
	buf[0] = '\0';
	DWORD len = sizeof(buf);
	webp_context *c = (webp_context *)malloc(sizeof(webp_context));
	if (!init_webp_context(c, &m->config)) {
		free(c);
		return KF_STATUS_REQ_FINISHED;
	}
	c->max_length = m->max_length;
	if (c->max_length <= 0) {
		c->max_length = MAX_WEBP_SIZE;
	}
	if (KGL_OK == ctx->f->get_variable(rq, KGL_VAR_QUERY_STRING, NULL, buf, &len)) {
		char *p = NULL;
		if (strncmp(buf, "_wpq=", 5) == 0) {
			p = buf + 5;
		} else {
			p = strstr(buf, "&_wpq=");
			if (p) {
				p += 6;
			}
		}
		if (p) {
			int q = atoi(p);
			if (q >= 100) {
				free(c);
				return KF_STATUS_REQ_FALSE;
			}
			WebPConfigPreset(&c->config, WEBP_PRESET_DEFAULT, (float)q);
		}
	}
	len = sizeof(buf);
	buf[0] = '\0';
	if (KGL_OK == ctx->f->get_variable(rq, KGL_VAR_HEADER, "Accept", buf, &len) && is_support_webp(buf)) {
		c->accept_support = 1;
	}
	if (!register_upstream(rq, ctx, c)) {
		free(c);
		return KF_STATUS_REQ_FALSE;
	}
	return KF_STATUS_REQ_TRUE;
}
static kgl_access access_model = {
	sizeof(kgl_access),
	KF_NOTIFY_REQUEST_MARK,
	"webp",
	create_ctx,
	free_ctx,
	build,
	parse,
	process,
	NULL
};
void register_access(kgl_dso_version *ver)
{
	KGL_REGISTER_ACCESS(ver, &access_model);
}
