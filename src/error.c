#include "error.h"
#include <stdint.h>

volatile uint8_t* BORDERCOLOR = (uint8_t*)0xD020;

// flash the border color red and black and just hang
void guru()
{
    while (1)
    {
        *BORDERCOLOR = 2;
        for (uint32_t i = 0; i < 100000; i++)
            ;
        *BORDERCOLOR = 0;
        for (uint32_t i = 0; i < 100000; i++)
            ;
    }
}