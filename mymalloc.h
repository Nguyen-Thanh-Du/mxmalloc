#ifndef MYMALLOC_H
#define MYMALLOC_H

#include <stddef.h>

void *malloc(size_t request_size);
void *realloc(void *block, size_t request_size);
void *calloc(size_t num_element, size_t element_size);
void free(void *block);

#endif /* MYMALLOC_H */