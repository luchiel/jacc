#ifndef JACC_BUFFER_H
#define JACC_BUFFER_H

typedef struct buffer_tag {
	char *buf;
	int used;
	int size;
} *buffer_t;

extern buffer_t buffer_create(int initial_size);
extern void buffer_append(buffer_t buffer, char c);
extern void buffer_append_string(buffer_t buffer, char *src, int len);
extern void buffer_reset(buffer_t buffer);

extern int buffer_size(buffer_t buffer);
extern char *buffer_data(buffer_t buffer);
extern char *buffer_data_copy(buffer_t buffer);
extern void buffer_free(buffer_t buffer);

#endif