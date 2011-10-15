#include <stdlib.h>
#include <stdio.h>
#include "memory.h"

extern void *jacc_check_malloc_result(void *ptr)
{
	if (ptr == NULL) {
		fprintf(stderr, "Memory allocation failed");
		exit(EXIT_FAILURE);
	}
	return ptr;
}