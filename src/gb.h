/* gb.h - shared types, globals and function prototypes */
#ifndef GB_H
#define GB_H

#include <stdint.h>
#include <stddef.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;

/* CPU flag bits in register F */
enum
{
    FZ = 0x80,
    FN = 0x40,
    FH = 0x20,
    FC = 0x10
};

/* memory.c */
extern u8 vram[0x2000], oam[0xA0], io[0x80], ie;
extern u8 joy_dir, joy_btn; /* joypad state, 0 bit = pressed */
int load_rom(const char *path);
void mem_reset(void);
u8 rb(u16 addr);
void wb(u16 addr, u8 val);

/* cpu.c */
void cpu_reset(void);
int cpu_step(void); /* runs one instruction/interrupt, returns T-cycles */

/* timer.c */
extern u16 divc;
void timer_reset(void);
void timer_tick(int cycles);

/* gpu.c */
extern u32 fb[160 * 144]; /* ARGB8888 framebuffer */
extern int frame_ready, dots, ppu_mode;
void gpu_reset(void);
void ppu_tick(int cycles);

#endif
