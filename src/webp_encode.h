#ifndef DSO_KWEBPENCODER_H_99
#define DSO_KWEBPENCODER_H_99
#include "upstream.h"
#include "ksapi.h"
bool webp_read_picture(webp_context *webp_ctx);
KGL_RESULT webp_encode_picture(kgl_async_context *ctx);
#ifndef _WIN32
int unix_read_picture(char *data,int len,struct WebPPicture* const pic);
KGL_RESULT gif2webp(webp_context *webp,WebPData *webp_data);
#endif
#endif
