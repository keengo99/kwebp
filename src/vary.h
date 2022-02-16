#ifndef DSO_WEBP_VARY_H_99
#define DSO_WEBP_VARY_H_99
#include "ksapi.h"
bool is_support_webp(const char *accept);
void register_vary(kgl_dso_version *ver);
#endif
