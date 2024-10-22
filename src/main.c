#include <stdio.h>
#include <stdlib.h>

#include "error.h"
#include "memory.h"

// Call the BSOUT routine to output a character
void bsout(char x)
{
    // clang-format off
    __asm(  " jsr 0xFFD2\n"
            : 
            : "Ka"(x) 
            : "a"
    );
    // clang-format on
}

int main()
{
    bsout(14);
    bsout(11);
    bsout(27);
    bsout(53);

    *(uint8_t*)0xD021 = 0;

    attic_init();
    attic_status();

    void __huge* ptr1 = attic_malloc(100);
    void __huge* ptr2 = attic_malloc(200);
    void __huge* ptr3 = attic_malloc(300);
    void __huge* ptr4 = attic_malloc(400);
    attic_status();

    attic_free(ptr2);
    attic_free(ptr3);
    void __huge* ptr5 = attic_malloc(5000);
    attic_free(ptr4);
    attic_free(ptr1);

    attic_status();

    attic_free(ptr5);
    attic_status();

    void __huge* ptr6 = attic_malloc(1000000);
    attic_status();

    return 0;
}
