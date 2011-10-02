#include <stdlib.h>
#include <string.h>
#include "buffer.h"

extern buffer_t buffer_create(int initial_size)
{
	buffer_t buffer = (buffer_t)malloc(sizeof(struct buffer_tag));
	if (buffer == NULL) {
		return NULL;
	}
	buffer->size = initial_size;
	buffer->used = 0;
	buffer->buf = (char*)malloc(initial_size);
	if (buffer->buf == NULL) {
		free(buffer);
		return NULL;
	}
	return buffer;
}

static inline void ensure_size(buffer_t buffer, int size)
{
	if (size > buffer->size) {
		buffer->buf = (char*)realloc(buffer->buf, size);
	}
}

extern void buffer_append(buffer_t buffer, char c)
{
	ensure_size(buffer, buffer->used + 1);
	buffer->buf[buffer->used] = c;
	buffer->used++;;
}

extern void buffer_append_string(buffer_t buffer, char *src, int len)
{
	ensure_size(buffer, buffer->used + len);
	memcpy(buffer->buf + buffer->used, src, len);
	buffer->used += len;
}

extern void buffer_reset(buffer_t buffer)
{
	buffer->used = 0;
}

extern int buffer_size(buffer_t buffer)
{
	return buffer->used;
}

extern char *buffer_data(buffer_t buffer)
{
	return buffer->buf;
}

extern char *buffer_data_copy(buffer_t buffer)
{
	char *buf = (char*)malloc(buffer->used);
	if (buf == NULL) {
		return NULL;
	}
	memcpy(buf, buffer->buf, buffer->used);
	return buf;
}

extern void buffer_free(buffer_t buffer)
{
	free(buffer->buf);
}