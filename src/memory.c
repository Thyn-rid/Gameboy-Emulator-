/* memory.c - cartridge, memory map, MBC banking, I/O registers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gb.h"

static u8 *rom;
static size_t rom_size;
u8 vram[0x2000], oam[0xA0], io[0x80], ie; /* shared with cpu.c / gpu.c */
static u8 eram[0x20000], wram[0x2000], hram[0x80];

static int mbc, rom_bank = 1, ram_bank, ram_en, mbc1_lo = 1, mbc1_hi, mbc1_mode;
u8 joy_dir = 0x0F, joy_btn = 0x0F; /* 0 bit = pressed */

static void mbc_write(u16 a, u8 v)
{
    switch (mbc)
    {
    case 1:
        if (a < 0x2000)
            ram_en = (v & 0xF) == 0xA;
        else if (a < 0x4000)
        {
            mbc1_lo = v & 0x1F;
            if (!mbc1_lo)
                mbc1_lo = 1;
        }
        else if (a < 0x6000)
            mbc1_hi = v & 3;
        else
            mbc1_mode = v & 1;
        rom_bank = mbc1_lo | (mbc1_hi << 5);
        ram_bank = mbc1_mode ? mbc1_hi : 0;
        break;
    case 3:
        if (a < 0x2000)
            ram_en = (v & 0xF) == 0xA;
        else if (a < 0x4000)
        {
            rom_bank = v & 0x7F;
            if (!rom_bank)
                rom_bank = 1;
        }
        else if (a < 0x6000)
            ram_bank = v & 3; /* RTC not emulated */
        break;
    case 5:
        if (a < 0x2000)
            ram_en = (v & 0xF) == 0xA;
        else if (a < 0x3000)
            rom_bank = (rom_bank & 0x100) | v;
        else if (a < 0x4000)
            rom_bank = (rom_bank & 0xFF) | ((v & 1) << 8);
        else if (a < 0x6000)
            ram_bank = v & 0xF;
        break;
    }
}

static u8 io_read(u16 a)
{
    switch (a)
    {
    case 0xFF00:
    {
        u8 sel = io[0] & 0x30, v = 0xC0 | sel | 0x0F;
        if (!(sel & 0x10))
            v &= 0xF0 | joy_dir;
        if (!(sel & 0x20))
            v &= 0xF0 | joy_btn;
        return v;
    }
    case 0xFF04:
        return divc >> 8;
    case 0xFF0F:
        return io[0x0F] | 0xE0;
    default:
        return io[a - 0xFF00];
    }
}

u8 rb(u16 a)
{
    if (a < 0x4000)
        return rom[a];
    if (a < 0x8000)
        return rom[((size_t)rom_bank * 0x4000 + (a - 0x4000)) % rom_size];
    if (a < 0xA000)
        return vram[a - 0x8000];
    if (a < 0xC000)
        return ram_en ? eram[ram_bank * 0x2000 + a - 0xA000] : 0xFF;
    if (a < 0xE000)
        return wram[a - 0xC000];
    if (a < 0xFE00)
        return wram[a - 0xE000];
    if (a < 0xFEA0)
        return oam[a - 0xFE00];
    if (a < 0xFF00)
        return 0xFF;
    if (a < 0xFF80)
        return io_read(a);
    if (a < 0xFFFF)
        return hram[a - 0xFF80];
    return ie;
}

static void io_write(u16 a, u8 v)
{
    switch (a)
    {
    case 0xFF00:
        io[0] = v & 0x30;
        break;
    case 0xFF02:
        io[2] = v;
        if (v == 0x81)
        {
            putchar(io[1]);
            fflush(stdout);
            io[2] = 0x01;
        }
        break;
    case 0xFF04:
        divc = 0;
        break;
    case 0xFF0F:
        io[0x0F] = v & 0x1F;
        break;
    case 0xFF40:
    {
        u8 old = io[0x40];
        io[0x40] = v;
        if ((old & 0x80) && !(v & 0x80))
        {
            io[0x44] = 0;
            dots = 0;
            ppu_mode = 0;
        }
        break;
    }
    case 0xFF41:
        io[0x41] = (io[0x41] & 7) | (v & 0x78);
        break;
    case 0xFF44:
        break; /* LY is read-only */
    case 0xFF46:
    { /* OAM DMA */
        u16 s = v << 8;
        for (int i = 0; i < 0xA0; i++)
            oam[i] = rb(s + i);
        io[0x46] = v;
        break;
    }
    default:
        io[a - 0xFF00] = v;
    }
}

void wb(u16 a, u8 v)
{
    if (a < 0x8000)
        mbc_write(a, v);
    else if (a < 0xA000)
        vram[a - 0x8000] = v;
    else if (a < 0xC000)
    {
        if (ram_en)
            eram[ram_bank * 0x2000 + a - 0xA000] = v;
    }
    else if (a < 0xE000)
        wram[a - 0xC000] = v;
    else if (a < 0xFE00)
        wram[a - 0xE000] = v;
    else if (a < 0xFEA0)
        oam[a - 0xFE00] = v;
    else if (a < 0xFF00)
    {
    }
    else if (a < 0xFF80)
        io_write(a, v);
    else if (a < 0xFFFF)
        hram[a - 0xFF80] = v;
    else
        ie = v;
}

int load_rom(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp)
    {
        perror(path);
        return 0;
    }
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    rom_size = n < 0x8000 ? 0x8000 : (size_t)n;
    rom = calloc(rom_size, 1);
    if (fread(rom, 1, n, fp) != (size_t)n)
    {
        fprintf(stderr, "short read\n");
        return 0;
    }
    fclose(fp);

    u8 t = rom[0x147];
    if (t >= 1 && t <= 3)
        mbc = 1;
    else if (t >= 0x0F && t <= 0x13)
        mbc = 3;
    else if (t >= 0x19 && t <= 0x1E)
        mbc = 5;
    else
        mbc = 0;
    char title[17] = {0};
    memcpy(title, rom + 0x134, 16);
    fprintf(stderr, "ROM: \"%s\"  type=0x%02X (MBC%d)  size=%zu KB\n", title, t, mbc, rom_size / 1024);
    return 1;
}

void mem_reset(void)
{ /* post-boot I/O state (no BIOS needed) */
    io[0x40] = 0x91;
    io[0x47] = 0xFC;
    io[0x48] = 0xFF;
    io[0x49] = 0xFF;
    io[0x0F] = 0x01;
}
