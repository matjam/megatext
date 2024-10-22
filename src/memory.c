#include "memory.h"
#include "error.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// MEGA65 Attic Allocator
//
// The attic allocator works by maintaining a linked list of allocations where the header of each block
// contains the size of the block, the status of the block (allocated or free), and pointers to the previous
// and next blocks. By strictly keeping the order of the blocks in the list, we can easily merge adjacent
// free blocks and split used blocks when allocating memory.
//
// The attic allocator is a simple allocator that is not optimized for speed or fragmentation though given
// the nature of the system, fragmentation should be kept to a minimum and the speed of the allocator should
// be sufficient for most applications given that the MEGA65 is not a multitasking system.
//
// The blocks are arranged such that each block_t is stored directly before the memory that is allocated. We
// must take care to ensure that when splitting and merging blocks, that we take into account the size of the
// block_t header. Therefore, when creating a block, we add the size of the block_t header and align the size
// of the block to the BLOCK_ALIGN boundary, which is 32 bytes.
//
// This looks like the following:
//
// +-----------------+-----------------+-----------------+-----------------+
// | block_t header  |   allocated     |   allocated     |   allocated     |
// +-----------------+-----------------+-----------------+-----------------+
// | allocated       |   block_t header|   free          |   free          |
// +-----------------+-----------------+-----------------+-----------------+
//
// The attic allocator is initialized by calling attic_init() which sets up the attic structure and creates a
// single block that represents the entire memory region. When memory is allocated, attic_malloc() is called
// with the size of the memory to allocate. The allocator will then search for a free block that is large enough
// to hold the requested memory. If the block is larger than the requested memory, the block is split into two
// blocks, one that is the size of the requested memory and the other that is the remaining memory. The block
// that is allocated is then marked as used and the pointer to the memory is returned. If no block is found, then
// NULL is returned.
//
// When memory is freed, attic_free() is called with the pointer to the memory that was allocated. The block is
// then marked as free and the block is merged with any adjacent free blocks. This is done by iterating over the
// block list and checking if the current block is free and the next block is free. If so, the two blocks are merged
// into a single block.

#define BLOCK_ALIGN 32

#define ATTIC_BASE (uint32_t)0x08000000             // Start of the attic
#define ATTIC_SIZE (uint32_t)0x7FFFFF - BLOCK_ALIGN // 8MB
#define sizeof32(x) (uint32_t)sizeof(x)             // sizeof for 32-bit pointer math

#define BLOCK_FREE 0
#define BLOCK_USED 1

typedef struct block block_t;
struct block
{
    uint8_t status;       // 0 = free, 1 = used
    uint32_t size;        // size of the block excluding the header
    block_t __huge* prev; // previous block in the list
    block_t __huge* next; // next block in the list
};

typedef struct attic attic_t;
struct attic
{
    uint32_t free_size;    // total size of all of the allocated blocks in this list
    uint32_t used_size;    // total size of all of the free blocks in this list
    uint32_t free_count;   // total number of allocated blocks in the list
    uint32_t used_count;   // total number of free blocks in the list
    block_t __huge* first; // first block in the list
    block_t __huge* last;  // last block in the list
};

// A pointer to the attic that stores all of the memory allocations, as well
// as the state of the memory allocations.
attic_t __huge* attic = (attic_t __huge*)ATTIC_BASE;

#define BUFFER_LEN 128
char buffer[BUFFER_LEN];

char* block_info(block_t __huge* block)
{
    if (block == 0)
    {
        return "NULL";
    }

    if (block->status == BLOCK_FREE)
    {
        snprintf(buffer, BUFFER_LEN - 1, "free %08lx to %08lx PREV %08lx NEXT %08lx (%lu bytes)", (uint32_t)block,
                 (uint32_t)block + block->size - 1, (uint32_t)block->prev, (uint32_t)block->next, block->size);
    }
    else
    {
        snprintf(buffer, BUFFER_LEN - 1, "used %08lx to %08lx PREV %08lx NEXT %08lx (%lu bytes)", (uint32_t)block,
                 (uint32_t)block + block->size - 1, (uint32_t)block->prev, (uint32_t)block->next, block->size);
    }

    return buffer;
}

char* attic_info()
{
    snprintf(buffer, BUFFER_LEN - 1, "%lu bytes free WITH %lu ALLOCATIONS", attic->free_size, attic->used_count);

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

// Align to multiples of BLOCK_ALIGN bytes
uint32_t align(uint32_t size)
{
    return (size + BLOCK_ALIGN - 1) & ~(BLOCK_ALIGN - 1);
}

// Append a block to the end of the block list
void block_list_append(block_t __huge* block)
{
    if (attic->first == 0)
    {
        attic->first = block;
        attic->last = block;
    }
    else
    {
        block->prev = attic->last;
        attic->last->next = block;
        attic->last = block;
    }

    return;
}

// insert a block after another block
void block_list_insert_after(block_t __huge* block, block_t __huge* new_block)
{
    new_block->prev = block;
    new_block->next = block->next;

    if (block->next != 0)
    {
        block->next->prev = new_block;
    }

    block->next = new_block;

    if (attic->last == block)
    {
        attic->last = new_block;
    }

    return;
}

// Remove a block from the block list
void block_list_remove(block_t __huge* block)
{
    if (block->prev != 0)
    {
        block->prev->next = block->next;
    }

    if (block->next != 0)
    {
        block->next->prev = block->prev;
    }

    if (attic->first == block)
    {
        attic->first = block->next;
    }

    if (attic->last == block)
    {
        attic->last = block->prev;
    }

    return;
}

// Create a new block. The address parameter is the address of the block in memory, the size parameter is the
// size of the block, and the status parameter is the status of the block (BLOCK_FREE or BLOCK_USED).
// size includes the size of the block_t header.
block_t __huge* block_new(uint32_t address, uint32_t size, uint8_t status)
{
    block_t __huge* block = (block_t __huge*)address;
    block->status = status;
    block->size = size;
    block->prev = 0;
    block->next = 0;

    return block;
}

// Split a block into two blocks, by reszing the first block to the specified size and creating a new block
// with the remaining size. The size parameter includes the size of the block_t header. The function returns
// a pointer to the now smaller block.
block_t __huge* block_split(block_t __huge* block, uint32_t size)
{
    if (block->size < size)
    {
        printf("BLOCK SPLIT: BLOCK SIZE %lu IS LESS THAN SIZE %lu\r", block->size, size);
        guru();
    }

    uint32_t free_block_address = (uint32_t)((uint32_t)block + size);
    uint32_t free_block_size = (uint32_t)(block->size - size);

    block->size = size;

    // update the attic state
    attic->free_size -= block->size;
    attic->used_size += block->size;
    attic->used_count++;

    block_t __huge* free_block = block_new(free_block_address, free_block_size, BLOCK_FREE);
    block_list_insert_after(block, free_block);

    return block;
}

void block_merge(block_t __huge* block1, block_t __huge* block2)
{
    if (block1->status != BLOCK_FREE || block2->status != BLOCK_FREE)
    {
        printf("BLOCK MERGE: BLOCKS %08lx AND %08lx MUST BOTH BE FREE\r", (uint32_t)block1, (uint32_t)block2);
        guru();
    }

    block1->size += block2->size;
    block_list_remove(block2);

    // update the attic state; free size remains the same, but the count of free blocks decreases
    attic->free_count--;

    return;
}

// Find all adjacent free blocks and merge them into a single block
void block_merge_all()
{
    for (block_t __huge* block = attic->first; block != 0; block = block->next)
    {
        if (block->status == BLOCK_FREE && block->next != 0 && block->next->status == BLOCK_FREE)
        {
            block->size += block->next->size;
            block_list_remove(block->next);
        }
    }

    return;
}

// Initialize the attic
void attic_init()
{
    memset(buffer, 0, BUFFER_LEN);
    attic_memset(attic, 0, sizeof32(attic_t));

    // Create the initial block
    uint32_t initial_block_size = align(ATTIC_SIZE - sizeof32(attic_t) - sizeof32(block_t));
    uint32_t initial_block_address = align((uint32_t)attic + sizeof32(attic_t) + sizeof32(block_t));
    block_t __huge* block = block_new(initial_block_address, initial_block_size, BLOCK_FREE);
    block_list_append(block);

    attic->free_size = initial_block_size;
    attic->free_count = 1;

    return;
}

void attic_status()
{
    printf("--------------------------------------------------------------------------------\r");
    printf("ATTIC %s\r", attic_info());
    printf(" BLOCKS\r");

    for (block_t __huge* block = attic->first; block != 0; block = block->next)
    {
        printf("  %s\r", block_info(block));

        if (block->next == 0)
        {
            break;
        }
    }

    printf("--------------------------------------------------------------------------------\r");
}

void __huge* _attic_malloc(uint32_t size)
{
    // scan for a block large enough to hold the requested memory. If we find one, we keep a pointer to it
    // and continue scanning to see if there's an exact match. If we find an exact match, we return the block,
    // otherwise we split the block we found previously and return the new block.

    block_t __huge* block = 0;
    block_t __huge* exact_block = 0;

    for (block_t __huge* current = attic->first; current != 0; current = current->next)
    {
        if (current->status == BLOCK_FREE && current->size >= size)
        {
            if (current->size == size)
            {
                exact_block = current;
                break;
            }

            block = current;
        }
    }

    if (exact_block != 0)
    {
        exact_block->status = BLOCK_USED;

        return (void __huge*)((uint32_t)exact_block + sizeof32(block_t));
    }

    if (block != 0)
    {
        block->status = BLOCK_USED;

        return (void __huge*)((uint32_t)block_split(block, size) + sizeof32(block_t));
    }

    return 0;
}

// Allocate memory from the attic. The size parameter is the size of the memory to allocate.
// Returns a pointer to the allocated memory or NULL if no memory is available.
void __huge* attic_malloc(uint32_t size)
{
    // by aligning the size, we can ensure that the block is aligned to the BLOCK_ALIGN boundary
    // which makes it more likely that block can be reused, as well as making it easier to merge
    // adjacent blocks. We also add the size of the block_t header to the size of the block to ensure
    // that the block is large enough to hold the header and the requested memory.
    size = align(size + sizeof32(block_t));

    void __huge* ptr = _attic_malloc(size);

    if (ptr == 0)
    {
        // no block was found, try to merge adjacent blocks and try again
        block_merge_all();

        ptr = _attic_malloc(size);
    }

    return ptr;
}

void attic_free(void __huge* ptr)
{
    block_t __huge* block = (block_t __huge*)((uint32_t)ptr - sizeof32(block_t));

    if (block->status == BLOCK_FREE)
    {
        printf("BLOCK %08lx IS ALREADY FREE\r", (uint32_t)block);
        guru();
    }

    block->status = BLOCK_FREE;

    // update the attic state
    attic->free_size += block->size;
    attic->used_size -= block->size;
    attic->free_count++;
    attic->used_count--;

    // check if we can merge adjacent blocks, just this block the previous block and the next block
    if (block->prev != 0 && block->prev->status == BLOCK_FREE)
    {
        block_merge(block->prev, block);
        block = block->prev; // I'M THE BLOCK NOW BITCH
    }

    if (block->next != 0 && block->next->status == BLOCK_FREE)
    {
        block_merge(block, block->next);
    }

    return;
}
