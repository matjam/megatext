#include <stdio.h>
#include <stdlib.h>

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
    __attribute__((huge)) uint8_t* ptr = attic_malloc(100);
    attic_status();
    attic_free(ptr);
    attic_status();

    return 0;
}
