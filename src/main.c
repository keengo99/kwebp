#include <stdlib.h>
#include "kforwin32.h"
#include "ksapi.h"
kgl_dso_version *server_support = NULL;
void register_access(kgl_dso_version *ver);
void register_vary(kgl_dso_version *ver);
DLL_PUBLIC BOOL  kgl_dso_init(kgl_dso_version *ver)
{
	if (!IS_KSAPI_VERSION_COMPATIBLE(ver->api_version)) {
		return FALSE;
	}
	ver->api_version = KSAPI_VERSION;
	ver->module_version = MAKELONG(4, 1);
	server_support = ver;
	register_access(ver);
	register_vary(ver);
	return TRUE;
}
DLL_PUBLIC BOOL  kgl_dso_finit(DWORD flag)
{
	return TRUE;
}
