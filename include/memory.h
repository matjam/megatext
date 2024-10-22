#ifndef __MEMORY_H__
#define __MEMORY_H__

#include <stddef.h>
#include <stdlib.h>

void attic_init();
void __huge* attic_malloc(uint32_t size);
void attic_free(void __huge* ptr);
void attic_status();

#endif /* __MEMORY_H__ */