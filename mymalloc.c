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

struct FreeNode
{
    MemoryHeader_t *fd;
    MemoryHeader_t *bk;
};
typedef struct FreeNode FreeNode_t;

/* ─── Globals ─────────────────────────────────────────────────────────────── */
pthread_mutex_t global_malloc_lock = PTHREAD_MUTEX_INITIALIZER;
MemoryHeader_t *memory_head;
MemoryHeader_t *free_blocks[4];

/* ─── Internal declarations ───────────────────────────────────────────────── */
size_t align_size(size_t size);
void *header_to_block(MemoryHeader_t *header);
MemoryHeader_t *block_to_header(void *block);
MemoryHeader_t *get_free_block(size_t request_size);
void split_free_block(MemoryHeader_t *header, size_t request_size);
void forward_coalescing(MemoryHeader_t *header);
MemoryHeader_t *backward_coalescing(MemoryHeader_t *header);
Footer_t *get_prev_footer(MemoryHeader_t *header);
void write_footer(MemoryHeader_t *header);
MemoryHeader_t *get_prev_header(MemoryHeader_t *header, size_t size);
MemoryHeader_t *get_next_header(MemoryHeader_t *header);
size_t size_to_bin(size_t size);
void insert_free(MemoryHeader_t *header);
void remove_free(MemoryHeader_t *header);

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
    size_t idx = size_to_bin(request_size);
    MemoryHeader_t *res = NULL;

    while (idx <= 3)
    {
        MemoryHeader_t *current = free_blocks[idx];
        while (current)
        {
            if (current->size >= request_size)
            {
                if (!res)
                    res = current;
                else
                    res = current->size < res->size ? current : res;
            }
            FreeNode_t *node = (FreeNode_t *)(current + 1);
            current = node->fd;
        }
        idx++;
        if (res)
            break;
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
    insert_free(new_block);

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
            remove_free(next_header);
            next_header = get_next_header(next_header);
        }
        else
            break;
    }
    write_footer(header);
}

MemoryHeader_t *backward_coalescing(MemoryHeader_t *header)
{
    if (!header->is_prev_allocated)
    {
        Footer_t *prev_footer = get_prev_footer(header);
        MemoryHeader_t *prev_header = get_prev_header(header, prev_footer->size);

        remove_free(prev_header);

        prev_header->size += sizeof(MemoryHeader_t) + header->size;

        write_footer(prev_header);

        return prev_header;
    }
    return header;
}

Footer_t *get_prev_footer(MemoryHeader_t *header)
{
    return (Footer_t *)((char *)header - sizeof(Footer_t));
}

void write_footer(MemoryHeader_t *header)
{
    Footer_t *footer = (Footer_t *)((char *)(header + 1) + header->size - sizeof(Footer_t));
    footer->size = header->size;
}

MemoryHeader_t *get_prev_header(MemoryHeader_t *header, size_t size)
{
    return (MemoryHeader_t *)((char *)header - size - sizeof(MemoryHeader_t));
}

MemoryHeader_t *get_next_header(MemoryHeader_t *header)
{
    if ((void *)((char *)(header + 1) + header->size) == sbrk(0))
        return NULL;
    else
        return (MemoryHeader_t *)((char *)(header + 1) + header->size);
}

size_t size_to_bin(size_t size)
{
    size_t idx = 3;
    if (size <= 32)
        idx = 0;
    else if (size <= 64)
        idx = 1;
    else if (size <= 128)
        idx = 2;

    return idx;
}

void insert_free(MemoryHeader_t *header)
{
    size_t idx = size_to_bin(header->size);
    FreeNode_t *node = (FreeNode_t *)(header + 1);
    node->bk = NULL;
    node->fd = free_blocks[idx];

    if (free_blocks[idx])
    {
        FreeNode_t *old_head = (FreeNode_t *)(free_blocks[idx] + 1);
        old_head->bk = header;
    }

    free_blocks[idx] = header;
}

void remove_free(MemoryHeader_t *header)
{
    if (!header) return;

    FreeNode_t *node = (FreeNode_t *)(header + 1);

    if (node->bk)
    {
        FreeNode_t *prev = (FreeNode_t *)(node->bk + 1);
        prev->fd = node->fd;
    }
    else
    {
        free_blocks[size_to_bin(header->size)] = node->fd;
    }
    if (node->fd)
    {
        FreeNode_t *next = (FreeNode_t *)(node->fd + 1);
        next->bk = node->bk;
    }
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
        remove_free(header);

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

    if ((char *)block + header->size == sbrk(0))
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
                    remove_free(header);
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
        header = backward_coalescing(header);
        insert_free(header);
    }
    pthread_mutex_unlock(&global_malloc_lock);
}
