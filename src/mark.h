#ifndef KWEBPMARK_H
#define KWEBPMARK_H
#include "ksapi.h"
#include "upstream.h"

typedef struct _webp_mark {
	WebPConfig config;
	int quality;
	int max_length;
} webp_mark;

#endif
