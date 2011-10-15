#include <stdlib.h>
#include <string.h>
#include "memory.h"
#include "buffer.h"

struct buffer_data {
	char *buf;
	int used;
	int size;
};

extern buffer_t buffer_create(int initial_size)
{
	struct buffer_data *buffer = malloc(sizeof(*buffer));
	buffer->size = initial_size;
	buffer->used = 0;
	buffer->buf = malloc(initial_size);
	return buffer;
}

extern void buffer_ensure_capacity(buffer_t buffer, int capacity)
{
	if (capacity > buffer->size) {
		buffer->buf = realloc(buffer->buf, capacity);
	}
}

extern void buffer_append(buffer_t buffer, char c)
{
	buffer_ensure_capacity(buffer, buffer->used + 1);
	buffer->buf[buffer->used] = c;
	buffer->used++;;
}

extern void buffer_append_string(buffer_t buffer, char *src, int len)
{
	buffer_ensure_capacity(buffer, buffer->used + len);
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
	char *buf = malloc(buffer->used);
	memcpy(buf, buffer->buf, buffer->used);
	return buf;
}

extern void buffer_free(buffer_t buffer)
{
	free(buffer->buf);
	free(buffer);
}