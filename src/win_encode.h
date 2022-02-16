#ifndef DSO_WEBP_IMAGEIO_WICDEC_H_
#define DSO_WEBP_IMAGEIO_WICDEC_H_
#ifdef _WIN32
#include <Windows.h>
#ifdef __cplusplus
extern "C" {
#endif

	struct Metadata;
	struct WebPPicture;
	// Returns true on success.
	int WindowsReadPicture(HGLOBAL data,struct WebPPicture* const pic);

#ifdef __cplusplus
}    // extern "C"
#endif
#endif
#endif  // WEBP_IMAGEIO_WICDEC_H_
