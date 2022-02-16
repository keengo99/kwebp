#include <string.h>
#include "ksapi.h"
bool is_support_webp(const char *accept)
{
	return strstr(accept, "image/webp") != NULL;
}
static bool response_vary(kgl_vary_context *ctx, const char *value)
{	
	ctx->write(ctx->cn, kgl_expand_string("Accept"));
	return true;
}
static bool build_vary(kgl_vary_context *ctx, const char *value)
{
	char buf[512];
	DWORD size = sizeof(buf);
	buf[0] = '\0';
	KGL_RESULT result = ctx->get_variable(ctx->cn, KGL_VAR_HEADER, "Accept", buf, &size);
	if (result == KGL_OK) {
		if (is_support_webp(buf)) {
			ctx->write(ctx->cn, kgl_expand_string("webp"));
		}
	}
	return true;
}
static kgl_vary vary = {
	"webp",
	build_vary,
	response_vary,
};
void register_vary(kgl_dso_version *ver)
{
	KGL_REGISTER_VARY(ver, &vary);
}
