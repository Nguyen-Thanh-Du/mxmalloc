#include "mymalloc.h"

#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdbool.h>

/* ─── Data structures ─────────────────────────────────────────────────────── */
struct MemoryHeader
{
    size_t size;
    bool is_allocated;
    bool is_prev_allocated;
};
typedef struct MemoryHeader MemoryHeader_t;

struct Footer
{
    size_t size;
};
typedef struct Footer Footer_t;

/* ─── Globals ─────────────────────────────────────────────────────────────── */
pthread_mutex_t global_malloc_lock = PTHREAD_MUTEX_INITIALIZER;
MemoryHeader_t *memory_head;

/* ─── Internal declarations ───────────────────────────────────────────────── */
size_t align_size(size_t size);
void *header_to_block(MemoryHeader_t *header);
MemoryHeader_t *block_to_header(void *block);
MemoryHeader_t *get_free_block(size_t request_size);
void split_free_block(MemoryHeader_t *header, size_t request_size);
void forward_coalescing(MemoryHeader_t *header);
void backward_coalescing(MemoryHeader_t *header);
Footer_t *get_prev_footer(MemoryHeader_t *header);
void write_footer(MemoryHeader_t *header);
MemoryHeader_t *get_prev_header(MemoryHeader_t *header, size_t size);
MemoryHeader_t *get_next_header(MemoryHeader_t *header);

/* ─── Internal helpers ────────────────────────────────────────────────────── */
size_t align_size(size_t size)
{
    return (size + 15) & ~15;
}

void *header_to_block(MemoryHeader_t *header)
{
    return (void *)(header + 1);
}

MemoryHeader_t *block_to_header(void *block)
{
    return (MemoryHeader_t *)block - 1;
}

MemoryHeader_t *get_free_block(size_t request_size)
{
    MemoryHeader_t *res = NULL;
    MemoryHeader_t *tmp = memory_head;

    while (tmp)
    {
        if (!tmp->is_allocated && tmp->size >= request_size)
        {
            if (!res)
                res = tmp;
            else
                res = tmp->size < res->size ? tmp : res;
        }
        tmp = get_next_header(tmp);
    }
    return res;
}

void split_free_block(MemoryHeader_t *header, size_t request_size)
{
    if ((header->size - request_size) <= (sizeof(MemoryHeader_t) + 16))
    {
        MemoryHeader_t *next_block = get_next_header(header);
        next_block->is_prev_allocated = true;
        return;
    }
    MemoryHeader_t *new_block = (char *)(header + 1) + request_size;
    new_block->is_allocated = false;
    new_block->size = header->size - request_size - sizeof(MemoryHeader_t);
    new_block->is_prev_allocated = true;
    write_footer(new_block);

    MemoryHeader_t *next_block = get_next_header(header);
    next_block->is_prev_allocated = false;

    header->size = request_size;
}

void forward_coalescing(MemoryHeader_t *header)
{
    MemoryHeader_t *next_header = get_next_header(header);
    while (next_header)
    {
        if (!next_header->is_allocated)
        {
            header->size += sizeof(MemoryHeader_t) + next_header->size;
            next_header = get_next_header(next_header);
        }
        else
            break;
    }
    write_footer(header);
}

void backward_coalescing(MemoryHeader_t *header)
{
    if (!header->is_prev_allocated)
    {
        Footer_t *prev_footer = get_prev_footer(header);
        MemoryHeader_t *prev_header = get_prev_header(header, prev_footer->size);

        prev_header->size += sizeof(MemoryHeader_t) + header->size;

        write_footer(prev_header);
    }
}

Footer_t *get_prev_footer(MemoryHeader_t *header)
{
    return (Footer_t *)((char*)header - sizeof(Footer_t));
}

void write_footer(MemoryHeader_t *header)
{
    Footer_t *footer = (Footer_t*)((char*)(header + 1) + header->size - sizeof(Footer_t));
    footer->size = header->size;
}

MemoryHeader_t *get_prev_header(MemoryHeader_t *header, size_t size)
{
    return (MemoryHeader_t *)((char*)header - size - sizeof(MemoryHeader_t));
}

MemoryHeader_t *get_next_header(MemoryHeader_t *header) {
    if ((void *)((char*)(header + 1) + header->size) == sbrk(0))
        return NULL;
    else
        return (MemoryHeader_t *)((char*)(header + 1) + header->size);
}

/* ─── Public API ──────────────────────────────────────────────────────────── */
void *malloc(size_t request_size)
{

    if (!request_size)
        return NULL;

    request_size = align_size(request_size);

    pthread_mutex_lock(&global_malloc_lock);

    MemoryHeader_t *header = get_free_block(request_size);

    if (header)
    {
        split_free_block(header, request_size);

        header->is_allocated = true;

        pthread_mutex_unlock(&global_malloc_lock);

        return header_to_block(header);
    }
    // else
    void *temp = sbrk(request_size + sizeof(MemoryHeader_t));
    if (temp == (void *)-1)
    {
        pthread_mutex_unlock(&global_malloc_lock);
        return NULL;
    }
    header = temp;

    header->size = request_size;
    header->is_allocated = true;
    header->is_prev_allocated = true;

    if (!memory_head)
    {
        memory_head = header;
    }

    pthread_mutex_unlock(&global_malloc_lock);
    return header_to_block(header);
}

void *realloc(void *block, size_t request_size)
{
    if (!block && request_size)
        return malloc(request_size);
    if (!request_size)
    {
        return NULL;
    }

    MemoryHeader_t *header = block_to_header(block);

    if (header->size >= request_size)
        return block;

    void *new_block = malloc(request_size);
    if (!new_block)
        return NULL;

    memcpy(new_block, block, header->size);

    free(block);

    return new_block;
}

void *calloc(size_t num_element, size_t element_size)
{
    if (!num_element || !element_size)
        return NULL;

    size_t total_size = num_element * element_size;
    if ((total_size / num_element) != element_size)
        return NULL;

    void *memory_block = malloc(total_size);
    if (!memory_block)
        return NULL;
    memset(memory_block, 0, total_size);

    return memory_block;
}

void free(void *block)
{
    if (!block)
        return;

    pthread_mutex_lock(&global_malloc_lock);
    MemoryHeader_t *header = block_to_header(block);

    if ((char*)block + header->size == sbrk(0))
    {
        if (memory_head == header)
        {
            memory_head = NULL;
            sbrk(-(intptr_t)(header->size + sizeof(MemoryHeader_t)));
        }
        else
        {
            size_t remove_size;

            do
            {
                remove_size = header->size;

                if (!header->is_prev_allocated)
                {
                    Footer_t *prev_footer = get_prev_footer(header);
                    header = get_prev_header(header, prev_footer->size);
                }
                else
                {
                    if (memory_head == header) 
                        memory_head = header = NULL;
                    else
                        header = NULL;
                }
                sbrk(-(intptr_t)(remove_size + sizeof(MemoryHeader_t)));

            } while (header);

            pthread_mutex_unlock(&global_malloc_lock);
            return;
        }
    }
    else
    {
        header->is_allocated = false;
        MemoryHeader_t *next_header = get_next_header(header);
        next_header->is_prev_allocated = false;
        forward_coalescing(header);
        backward_coalescing(header);
    }
    pthread_mutex_unlock(&global_malloc_lock);
}
