#include <string.h>
#include <assert.h>
#include "upstream.h"
#include "webp_encode.h"
#define kgl_align(d, a)     (((d) + (a - 1)) & ~(a - 1))
#define WEBP_VARY "|webp"
bool realloc_buffer(webp_buffer* buf, int new_len);
bool add_buff_data(webp_buffer* buf, const char* data, int len, int max_length);
KGL_RESULT begin_response_header(kgl_async_context* ctx, int64_t body_size, kgl_response_body* body);
KGL_RESULT begin_response(kgl_async_context* ctx);

static KGL_RESULT kwebp_write(kgl_response_body_ctx* ctx, const char* buf, int size)
{
	kgl_async_context* async_ctx = kgl_get_out_async_context(ctx);
	webp_context* webp = (webp_context*)async_ctx->module;
	assert(!webp->no_encode);
	if (webp->buff.data == NULL && webp->content_length > 0) {
		realloc_buffer(&webp->buff, webp->content_length);
	}
	if (!add_buff_data(&webp->buff, buf, size, webp->max_length)) {
		return KGL_EINSUFFICIENT_BUFFER;
	}
	return KGL_OK;
}
static KGL_RESULT unsupport_writev(kgl_response_body_ctx* ctx, WSABUF* bufs, int bc)
{
	for (int i = 0; i < bc; i++) {
		KGL_RESULT result = kwebp_write(ctx, (char*)bufs[i].iov_base, bufs[i].iov_len);
		if (result != KGL_OK) {
			return result;
		}
	}
	return KGL_OK;
}

static KGL_RESULT kwebp_flush(kgl_response_body_ctx* ctx)
{
	return KGL_OK;
}
static bool kwebp_support_sendfile(kgl_response_body_ctx* ctx)
{
	return false;
}
static KGL_RESULT kwebp_close(kgl_response_body_ctx* gate, KGL_RESULT result)
{
	kgl_async_context* ctx = kgl_get_out_async_context(gate);
	webp_context* webp = (webp_context*)ctx->module;
	webp->upstream_was_body_finish = 1;
	webp->upstream_push_body_result = result;
	if (webp->send_header) {
		return webp->body.f->close(webp->body.ctx, result);
		//return ctx->out->f->write_end(ctx->out, rq, result);
	}
	if (result != KGL_OK) {
		webp->is_webp = 0;
		webp->no_encode = 1;
	}
	else if (webp->buff.used == 0) {
		webp->no_encode = 1;
	}
	if (webp->no_encode || !webp->is_webp) {
		return begin_response_header(ctx,-1,NULL);
	}
	if (!webp_read_picture(webp)) {
		webp->is_webp = 0;
		webp->no_encode = 1;
		return begin_response_header(ctx,-1,NULL);
	};
	return begin_response(ctx);
}
static kgl_response_body_function response_body_function = {
	unsupport_writev,
	kwebp_write,
	kwebp_flush,
	kwebp_support_sendfile,
	NULL,
	kwebp_close,
};
#ifndef _WIN32
bool realloc_buffer(webp_buffer *buf, int new_len)
{
	if (buf->data==NULL) {
		assert(buf->used == 0);
		buf->data = (char *)malloc(new_len);
		if (buf->data==NULL) {
			return false;
		}
	} else {
		char *new_data = realloc(buf->data, new_len);
		if (new_data==NULL) {
			return false;
		}
		buf->data = new_data;
	}
	return true;
}
#else
bool realloc_buffer(webp_buffer *buf, int new_len)
{
	if (buf->data==NULL) {
		buf->data = GlobalAlloc(GMEM_MOVEABLE, new_len);
		assert(buf->used == 0);
		if (buf->data == NULL) {
			return false;
		}
	} else {
		HGLOBAL new_data = GlobalReAlloc(buf->data, new_len, GMEM_MOVEABLE);
		if (new_data == NULL) {
			return false;
		}
		buf->data = new_data;
	}
	return true;
}
#endif
bool add_buff_data(webp_buffer *buf, const char *data, int len, int max_length)
{
	if (buf->left < len) {
		int new_len = buf->used + len;
		if (new_len > max_length) {
			//too big
			return false;
		}
		int align_len = 2 * buf->used;
		align_len = kgl_align(align_len, 4096);
		new_len = KGL_MAX(new_len, align_len);
		if (!realloc_buffer(buf, new_len)) {
			return false;
		}
		buf->left = new_len - buf->used;
		assert(buf->left >= len);
	}
#ifdef _WIN32
	char *image_mem = (char *)GlobalLock(buf->data);
	memcpy(image_mem + buf->used, data, len);
	GlobalUnlock(image_mem);
#else
	memcpy(buf->data + buf->used, data, len);
#endif
	buf->left -= len;
	buf->used += len;
	assert(buf->left >= 0);
	return true;
}
void push_status(kgl_output_stream_ctx *gate, uint16_t status_code) {
	kgl_async_context *ctx = kgl_get_out_async_context(gate);
	webp_context *webp = (webp_context *)ctx->module;
	if (status_code != 200) {
		webp->no_encode = 1;
		webp->is_webp = 0;
	}
	if (status_code == 304) {
#if 0
		KHTTPOBJECT obj = server_support->obj->get_old_obj(rq);
		if (obj != NULL) {
			char old_vary[255];
			DWORD size = sizeof(old_vary);
			if (KGL_OK == server_support->obj->get_header(obj, "Vary", old_vary, &size)) {
				if (strstr(old_vary, WEBP_VARY)!=NULL) {
					webp->no_encode = 1;
					webp->is_webp = 1;
				}
			}
		}
#endif
	}
	ctx->out->f->write_status(ctx->out->ctx, status_code);
}
KGL_RESULT push_unknow_header(kgl_output_stream_ctx*gate, const char *attr, hlen_t attr_len, const char *val, hlen_t val_len)
{
	kgl_async_context *ctx = kgl_get_out_async_context(gate);
	return ctx->out->f->write_unknow_header(ctx->out->ctx, attr, attr_len, val, val_len);
}
static KGL_RESULT push_trailer(kgl_output_stream_ctx* gate, const char* attr, hlen_t attr_len, const char* val, hlen_t val_len) {
	kgl_async_context* ctx = kgl_get_out_async_context(gate);
	return ctx->out->f->write_trailer(ctx->out->ctx, attr, attr_len, val, val_len);
}
KGL_RESULT push_header(kgl_output_stream_ctx*gate,kgl_header_type attr, const char *val, int val_len)
{
	kgl_async_context *ctx = kgl_get_out_async_context(gate);
	webp_context *webp = (webp_context *)ctx->module;
	if (webp->no_encode) {
		return ctx->out->f->write_header(ctx->out->ctx, attr, val, val_len);
	}
	switch (attr) {
	case kgl_header_content_length:
	{
		/*
		if (val_len == KGL_HEADER_VALUE_INT64) {
			INT64* content_length = (INT64*)val;
			if (webp->buff.data) {
				free(webp->buff.data);
				webp->buff.data = NULL;
			}
			webp->content_length = (int)(*content_length);

			if (webp->content_length > webp->max_length) {
				webp->no_encode = 1;
				return ctx->out->f->write_header(ctx->out->ctx, attr, val, val_len);
			}
		}
		*/
		return KGL_OK;
	}
	case kgl_header_content_type:
	{
		if (strncasecmp(val, kgl_expand_string("image/")) == 0 && strcasecmp(val, "image/webp") != 0) {
			//result = init_webp_reader(webp, WEBP_JPEG_FORMAT);
			/*
			if (strcasecmp(val, "image/gif") == 0) {
				//gif特殊处理
				webp->is_gif = 1;
				//printf("content-type is gif\n");
			}
			*/
			webp->is_webp = 1;
			if (webp->accept_support) {
				if (webp->origin_content_type) {
					free(webp->origin_content_type);
				}
				//把content-type头保存，未来如果转码失败，可以发回原来的头.
				webp->origin_content_type = kgl_strndup(val, val_len);
				return KGL_OK;
			}
		}
		webp->no_encode = 1;
		buffer_destroy(&webp->buff);
		break;
	}
	case kgl_header_vary:
	{
		if (webp->vary == NULL) {
			webp->vary = (char*)malloc(val_len + 1);
			memcpy(webp->vary, val, val_len);
			webp->vary[val_len] = '\0';
			webp->vary_len = val_len;
		}
		return KGL_OK;
	}
	default:
		break;
	}
	return ctx->out->f->write_header(ctx->out->ctx, attr, val, val_len);
}

KGL_RESULT begin_response_header(kgl_async_context *ctx, int64_t body_size, kgl_response_body *body)
{
	webp_context *webp = (webp_context *)ctx->module;
	webp->send_header = 1;
	if (webp->is_webp) {
		//KD_REQ_OBJ_IDENTITY 提高缓存命中率
		kgl_url *url = ctx->in->f->get_url(ctx->in->ctx);
		url->accept_encoding = (uint8_t)(~0);
	
		//是webp就要发送vary头
		if (webp->vary == NULL) {
			ctx->out->f->write_header(ctx->out->ctx, kgl_header_vary, kgl_expand_string(WEBP_VARY));
		} else {
			int new_len = webp->vary_len + sizeof(WEBP_VARY) + 3;
			char *vary = (char *)malloc(new_len);
			char *hot = vary;
			memcpy(hot, webp->vary, webp->vary_len);
			hot += webp->vary_len;
			memcpy(hot, kgl_expand_string(", "));
			hot += 2;
			memcpy(hot, kgl_expand_string(WEBP_VARY));
			hot += sizeof(WEBP_VARY)-1;
			*hot = '\0';
			ctx->out->f->write_header(ctx->out->ctx, kgl_header_vary, vary,(hlen_t)(hot-vary));
			free(vary);
		}
	}
	if (webp->no_encode) {
		//发回原来的content_type,vary,还有content_length;
		if (webp->vary) {
			ctx->out->f->write_header(ctx->out->ctx, kgl_header_vary, webp->vary, webp->vary_len);
		}
		if (webp->origin_content_type) {
			ctx->out->f->write_header(ctx->out->ctx ,kgl_header_content_type, webp->origin_content_type, (hlen_t)strlen(webp->origin_content_type));
		}
		return ctx->out->f->write_header_finish(ctx->out->ctx, body_size, body);
	}
	ctx->out->f->write_header(ctx->out->ctx, kgl_header_content_type, kgl_expand_string("image/webp"));
	return ctx->out->f->write_header_finish(ctx->out->ctx, -1, &webp->body);
}
KGL_RESULT push_header_finish(kgl_output_stream_ctx*gate, int64_t body_size, kgl_response_body *body)
{
	kgl_async_context *ctx = kgl_get_out_async_context(gate);
	webp_context *webp = (webp_context *)ctx->module;
	if (webp->buff.data) {
		free(webp->buff.data);
		webp->buff.data = NULL;
	}
	if (body_size > webp->max_length) {
		webp->no_encode = 1;
	} else {
		webp->content_length = (int)body_size;
	}
	if (!webp->is_webp) {
		//在header发送完，还没收到content-type
		webp->no_encode = 1;
	}
	if (webp->no_encode) {
		return begin_response_header(ctx, body_size, body);
	}
	body->ctx = (kgl_response_body_ctx*)ctx;
	body->f = &response_body_function;
	return KGL_OK;
}

KGL_RESULT begin_response(kgl_async_context* ctx)
{
	webp_context* webp = (webp_context*)ctx->module;
	KGL_RESULT result = begin_response_header(ctx, -1,NULL);
	if (result != KGL_OK) {
		return result;
	}
	if (webp->no_encode) {
		if (webp->buff.data == NULL) {
			return webp->body.f->close(webp->body.ctx, result);
		}
#ifdef _WIN32
		char* image_mem = (char*)GlobalLock(webp->buff.data);
		result = webp->body.f->write(webp->body.ctx,image_mem, webp->buff.used);
		GlobalUnlock(image_mem);
#else
		result = webp->body.f->write(webp->body.ctx, webp->buff.data, webp->buff.used);
#endif
		if (result == KGL_OK) {
			result = webp->upstream_push_body_result;
		}
		return webp->body.f->close(webp->body.ctx, result);
	}
	return webp->body.f->close(webp->body.ctx, webp_encode_picture(ctx));
}
KGL_RESULT handle_error(kgl_output_stream_ctx* gate, uint16_t status_code, const char* reason, size_t reason_len) {
	kgl_async_context* ctx = kgl_get_out_async_context(gate);
	webp_context* webp = (webp_context*)ctx->module;
	webp->no_encode = 1;
	webp->is_webp = 0;
	return ctx->out->f->error(ctx->out->ctx, status_code, reason, reason_len);
}

static kgl_output_stream_function push_gate_function = {
	push_status,
	push_header,
	push_unknow_header,
	handle_error,
	push_header_finish,
	push_trailer,
	NULL
};
void init_push_gate(kgl_async_context *ctx, kgl_output_stream *out)
{
	out->ctx = (kgl_output_stream_ctx *)ctx;
	out->f = &push_gate_function;
}
