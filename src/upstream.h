#ifndef KWEBPUPSTREAM_H_99
#define KWEBPUPSTREAM_H_99
#include <stdlib.h>
#include <stdbool.h>
#include "ksapi.h"
#include "webp/encode.h"
#include "webp/mux_types.h"
extern kgl_dso_version *server_support;
#ifndef _WIN32
typedef char * HGLOBAL;
#endif
typedef struct _webp_buffer {
	HGLOBAL data;
	int left;
	int used;
} webp_buffer;

typedef struct _webp_context {
	union {
		struct {
			/*
			is_webp	no_encode	����
			0		0			��ʼ״̬
			0		1			����webp(��Ȼ��ת��)
			1		1			��webp�����ͻ��˲�֧�֣�����״̬Ҫ����vary
			1		0			��webp������Ҳת��
			*/
			uint32_t is_webp:1;//�Ƿ���webp
			uint32_t no_encode : 1;//����Ҫת��webp
			uint32_t accept_support : 1;
			uint32_t upstream_was_body_finish : 1;
			uint32_t send_header : 1;
			uint32_t is_gif:1; //u�����ͣ��Ƿ���gif_data
		};
		uint32_t flags;
	};
	KGL_RESULT upstream_push_body_result;
	char *vary;
	int vary_len;
	char *origin_content_type;
	int content_length;
	int max_length;
	union {
		WebPPicture *picture;
		WebPData *gif_data;
		void *data;
	} u;
	WebPConfig config;
	webp_buffer buff;
	kgl_response_body body;
	KREQUEST rq;
} webp_context;

bool register_upstream(KREQUEST rq, kgl_access_context *ctx, webp_context *c);
void init_push_gate(kgl_async_context* ctx, kgl_output_stream* out);
bool init_webp_context(webp_context *ctx, WebPConfig *config);
static INLINE void buffer_init(webp_buffer *buf)
{
	buf->data = NULL;
	buf->used = 0;
	buf->left = 0;
}
static INLINE void buffer_destroy(webp_buffer *buf)
{
	if (buf->data!=NULL) {
#ifdef _WIN32
		GlobalFree(buf->data);
#else
		free(buf->data);
#endif
		buf->data = NULL;
		buf->left = 0;
		buf->used = 0;
	}
}
#endif
