#include "memory.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define ATTIC_BASE (uint32_t)0x08000000
#define ATTIC_SIZE (uint32_t)0x800000 // 8MB
#define sizeof32(x) (uint32_t)sizeof(x)

typedef struct block block_t;
struct block
{
    uint32_t size; // size of the block excluding the header
    block_t __huge* prev;
    block_t __huge* next;
}; // 16 bytes

typedef struct block_list block_list_t;
struct block_list
{
    uint32_t size;  // total size of all of the allocated blocks in this list
    uint32_t count; // total number of allocated blocks in the list
    block_t __huge* first;
    block_t __huge* last;
}; // 8 bytes

typedef struct attic attic_t;
struct attic
{
    block_list_t __huge* free_list;
    block_list_t __huge* used_list;
}; // 28 bytes

attic_t __huge* attic = (attic_t __huge*)ATTIC_BASE;

// Debugging
#ifdef MEMORY_DEBUG
#define DEBUG_PRINT(...)                                                                                               \
    if (MEMORY_DEBUG)                                                                                                  \
    {                                                                                                                  \
        printf(__VA_ARGS__);                                                                                           \
    }
#else
#define DEBUG_PRINT(...)
#endif

#define BUFFER_LEN 128
char buffer[BUFFER_LEN];

// The attic allocator works by maintaining a linked list of allocations where the header of each block
// contains the size of the block, the status of the block (allocated or free), and pointers to the previous
// and next blocks. The attic allocator is a simple allocator that assumes the CPU has no inline cache and
// that the memory is directly accessible.

char* block_info(block_t __huge* block)
{
    snprintf(buffer, BUFFER_LEN, "AT %lx SIZE %lu PREV %lx NEXT %lx", (uint32_t)block, block->size,
             (uint32_t)block->prev, (uint32_t)block->next);

    return buffer;
}

char* block_list_info(block_list_t __huge* list)
{
    snprintf(buffer, BUFFER_LEN, "SIZE %lu COUNT %lu FIRST %lx LAST %lx", list->size, list->count,
             (uint32_t)list->first, (uint32_t)list->last);

    return buffer;
}

char* attic_info()
{
    snprintf(buffer, BUFFER_LEN, "SIZE %lu FREE SIZE %lu FREE COUNT %lu USED SIZE %lu USED COUNT %lu", ATTIC_SIZE,
             attic->free_list->size, attic->free_list->count, attic->used_list->size, attic->used_list->count);

    return buffer;
}

// Set a memory region to a specific value
void attic_memset(void __huge* ptr, uint8_t value, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
    {
        *((uint8_t __huge*)((uint32_t)ptr + i)) = value;
    }
}

// Align to multiples of 32 bytes
uint32_t align(uint32_t size)
{
    return (size + 31) & ~31;
}

void block_list_append(block_list_t __huge* list, block_t __huge* block)
{
    if (list->first == 0)
    {
        list->first = block;
        list->last = block;
    }
    else
    {
        list->last->next = block;
        block->prev = list->last;
        list->last = block;
    }

    list->size += block->size;
    list->count++;

    return;
}

void block_list_remove(block_list_t __huge* list, block_t __huge* block)
{
    if (block->prev != 0)
    {
        block->prev->next = block->next;
    }

    if (block->next != 0)
    {
        block->next->prev = block->prev;
    }

    if (list->first == block)
    {
        list->first = block->next;
    }

    if (list->last == block)
    {
        list->last = block->prev;
    }

    list->size -= block->size;
    list->count--;

    return;
}

block_t __huge* block_new(uint32_t address, uint32_t size)
{
    block_t __huge* block = (block_t __huge*)address;
    block->size = size;
    block->prev = 0;
    block->next = 0;

    return block;
}

// Initialize the attic
void attic_init()
{
    attic_memset(attic, 0, sizeof32(attic_t));

    attic->free_list = (block_list_t __huge*)attic + sizeof32(attic_t);
    attic->used_list = attic->free_list + sizeof32(block_list_t);

    attic_memset(attic->free_list, 0, sizeof32(block_list_t));
    attic_memset(attic->used_list, 0, sizeof32(block_list_t));

    // add the remaining memory to the free list
    block_t __huge* block = block_new((uint32_t)attic + sizeof32(attic_t) + sizeof32(block_list_t) * 2,
                                      ATTIC_SIZE - (sizeof32(attic_t) - sizeof32(block_list_t) * 2));

    block_list_append(attic->free_list, block);

    return;
}

void attic_status()
{
    printf("----------------------------------------\r");
    printf("ATTIC %s\r", attic_info());
    printf(" FREE LIST %s\r", block_list_info(attic->free_list));
    block_t __huge* block = attic->free_list->first;
    while (block != 0)
    {
        printf("  BLOCK %s\r", block_info(block));
        block = block->next;
    }
    printf(" USED LIST %s\r", block_list_info(attic->used_list));
    block = attic->used_list->first;
    while (block != 0)
    {
        printf("  BLOCK %s\r", block_info(block));
        block = block->next;
    }
}

void __huge* attic_malloc(uint32_t size)
{
    size = align(size);

    // This allocatiom strategy is first fit, with a second pass to split blocks that are larger than the size we need
    // into two blocks. This is a simple strategy that is not very efficient but is easy to implement, but it will
    // fragment the memory over time. To fix that we will need to sort the free list and merge adjacent blocks, which
    // is a more complex strategy.

    block_t __huge* block = attic->free_list->first;

    // Check all free blocks to see if we can find a block that is exactly the size we need
    while (block != 0)
    {
        if (block->size == size)
        {
            block_list_remove(attic->free_list, block);
            block_list_append(attic->used_list, block);

            return (void __huge*)((uint32_t)block + sizeof32(block_t));
        }

        block = block->next;
    }

    // Check all free blocks to see if we can find a block that is larger than the size we need
    // and then we will split the block into two blocks
    block = attic->free_list->first;
    while (block != 0)
    {
        if (block->size > size)
        {
            block_list_remove(attic->free_list, block);
            block_t __huge* new_block = block_new((uint32_t)block + sizeof32(block_t) + size, block->size - size);
            block->size = size;
            block_list_append(attic->used_list, block);
            block_list_append(attic->free_list, new_block);

            return (void __huge*)((uint32_t)block + sizeof32(block_t));
        }

        block = block->next;
    }

    return 0;
}

void attic_free(void __huge* ptr)
{
    block_t __huge* block = (block_t __huge*)((uint32_t)ptr - sizeof32(block_t));

    block_list_remove(attic->used_list, block);
    block_list_append(attic->free_list, block);

    return;
}
