#include "webp_encode.h"
#undef HAVE_CONFIG_H
#ifndef _WIN32
#include "imageio/image_dec.h"

int unix_read_picture(char *data,int len,struct WebPPicture* const pic)
{
	WebPImageReader reader = WebPGuessImageReader(data, (size_t)len);
	return reader(data, len, pic, 1, NULL);
}
#endif