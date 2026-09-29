/* timer.c - DIV / TIMA / TMA / TAC */
#include "gb.h"

u16 divc; /* internal 16-bit DIV counter */

void timer_tick(int cycles)
{
    static const int bits[4] = {9, 3, 5, 7};
    for (int i = 0; i < cycles / 4; i++)
    {
        u16 old = divc;
        divc += 4;
        if (io[0x07] & 4)
        {
            int b = bits[io[0x07] & 3];
            if (((old >> b) & 1) && !((divc >> b) & 1))
            {
                if (++io[0x05] == 0)
                {
                    io[0x05] = io[0x06];
                    io[0x0F] |= 4;
                }
            }
        }
    }
}

void timer_reset(void) { divc = 0xABCC; }
