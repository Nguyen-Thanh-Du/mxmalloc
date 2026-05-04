#include <unistd.h>
#include <string.h>
#include <pthread.h>
/* Only for the debug printf */
#include <stdio.h>
#include <stdbool.h>

pthread_mutex_t global_malloc_lock = PTHREAD_MUTEX_INITIALIZER;

struct MemoryHeader
{
    size_t size;
    bool is_allocated;
    struct MemoryHeader *next;
};
typedef struct MemoryHeader MemoryHeader_t;


struct MemoryManager
{
    MemoryHeader_t *head;
    MemoryHeader_t *tail;
};
struct MemoryManager memory_manager;

size_t align_size(size_t size);
void *header_to_block(MemoryHeader_t *header);
MemoryHeader_t *block_to_header(void *block);
MemoryHeader_t *get_prev_header(MemoryHeader_t *header);
MemoryHeader_t *get_free_block(size_t request_size);
void split_free_block(MemoryHeader_t *header, size_t request_size);

void *malloc(size_t request_size);
void *realloc(void *block, size_t request_size);
void *calloc(size_t num_element, size_t element_size);
void free(void *block);

size_t align_size(size_t size)
{
    size_t align_size = (size + 15) & ~15;
    return align_size;
}

MemoryHeader_t *get_free_block(size_t request_size)
{
    MemoryHeader_t *current_header = memory_manager.head;
    while (current_header)
    {
        if (current_header->is_allocated == false && current_header->size >= request_size)
        {
            return current_header;
        }
        current_header = current_header->next;
    }
    return NULL;
}

void *header_to_block(MemoryHeader_t *header)
{
    return (void *)(header + 1);
}

MemoryHeader_t *block_to_header(void *block) {
    return (MemoryHeader_t*)block - 1;
}

void *malloc(size_t request_size)
{

    if (!request_size) return NULL;

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
    header->next = NULL;

    if (!memory_manager.head)
    {
        memory_manager.head = header;
        memory_manager.tail = header;
    }
    else
    {
        memory_manager.tail->next = header;
        memory_manager.tail = header;
    }

    pthread_mutex_unlock(&global_malloc_lock);
    return header_to_block(header);
}

void *realloc(void *block, size_t request_size)
{
    if (!block && request_size)
        return malloc(request_size);
    if (!request_size) {
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

MemoryHeader_t *get_prev_header(MemoryHeader_t *header)
{
    if (!header)
        return NULL;

    MemoryHeader_t *current = memory_manager.head;
    while (current)
    {
        if (current->next == header)
            return current;
        current = current->next;
    }
    return NULL;
}

void free(void *block)
{
    if (!block)
        return;

    pthread_mutex_lock(&global_malloc_lock);
    MemoryHeader_t *header = block_to_header(block);

    if (header == memory_manager.tail)
    {
        if (memory_manager.head == memory_manager.tail)
        {
            memory_manager.head = memory_manager.tail = NULL;
            sbrk(- (intptr_t)(header->size + sizeof(MemoryHeader_t)));
        }
        else
        {
            MemoryHeader_t *current = header;
            current->is_allocated = false;
            size_t remove_size;

            while (!current->is_allocated) {
                remove_size = current->size;

                current = get_prev_header(current);

                sbrk(- (intptr_t)(remove_size + sizeof(MemoryHeader_t)));
                
                if (current) {
                    memory_manager.tail = current;
                    memory_manager.tail->next = NULL;
                }
                else {
                    memory_manager.head = memory_manager.tail = NULL;
                    pthread_mutex_unlock(&global_malloc_lock);
                    return;
                }

            }
        }
    }
    else
    {
        header->is_allocatled = false;
    }
    pthread_mutex_unlock(&global_malloc_lock);
}

void *calloc(size_t num_element, size_t element_size) {
    if (!num_element || !element_size) return NULL;

    size_t total_size = num_element * element_size;
    if ((total_size / num_element) != element_size)
        return NULL;
    
    void *memory_block = malloc(total_size);
    if (!memory_block)
        return NULL;
    memset(memory_block, 0, total_size);

    return memory_block;
}

void split_free_block(MemoryHeader_t *header, size_t request_size) {
    if ((header->size - request_size) <= (sizeof(MemoryHeader_t) + 16)) return;

    MemoryHeader_t *new_block = (char*)(header + 1) + request_size;
    new_block->is_allocated = false;
    new_block->size = header->size - request_size - sizeof(MemoryHeader_t);
    new_block->next = header->next;
    
    header->next = new_block;
    header->size = request_size;
}
