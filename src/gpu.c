/* gpu.c - PPU: background, window, sprites, STAT/LY timing */
#include "gb.h"

u32 fb[160 * 144];
static const u32 palette[4] = {0xFFE0F8D0, 0xFF88C070, 0xFF346856, 0xFF081820};
int frame_ready, dots, ppu_mode;
static int win_line, stat_line;

static void render_line(int ly)
{
    u8 lcdc = io[0x40], scy = io[0x42], scx = io[0x43], wy = io[0x4A], wx = io[0x4B], bgp = io[0x47];
    u8 bgcol[160];
    int win_used = 0;
    int use_win = (lcdc & 0x20) && ly >= wy && wx <= 166;

    for (int x = 0; x < 160; x++)
    {
        u8 idx = 0;
        if (lcdc & 1)
        {
            int px, py;
            u16 map;
            if (use_win && x >= wx - 7)
            {
                px = x - (wx - 7);
                py = win_line;
                map = (lcdc & 0x40) ? 0x1C00 : 0x1800;
                win_used = 1;
            }
            else
            {
                px = (x + scx) & 255;
                py = (ly + scy) & 255;
                map = (lcdc & 8) ? 0x1C00 : 0x1800;
            }
            u8 tile = vram[map + (py / 8) * 32 + px / 8];
            u16 addr = (lcdc & 0x10) ? tile * 16 : 0x1000 + (s8)tile * 16;
            addr += (py & 7) * 2;
            int bit = 7 - (px & 7);
            idx = (((vram[addr + 1] >> bit) & 1) << 1) | ((vram[addr] >> bit) & 1);
        }
        bgcol[x] = idx;
        fb[ly * 160 + x] = palette[(bgp >> (idx * 2)) & 3];
    }
    if (win_used)
        win_line++;

    if (lcdc & 2)
    {
        int h = (lcdc & 4) ? 16 : 8, n = 0, list[10];
        for (int i = 0; i < 40 && n < 10; i++)
        {
            int sy = oam[i * 4] - 16;
            if (ly >= sy && ly < sy + h)
                list[n++] = i;
        }
        /* sort so the highest-priority sprite (lowest X, then lowest index) is drawn last */
        for (int a = 0; a < n; a++)
            for (int b = a + 1; b < n; b++)
            {
                int xa = oam[list[a] * 4 + 1], xb = oam[list[b] * 4 + 1];
                if (xb > xa || (xb == xa && list[b] > list[a]))
                {
                    int t = list[a];
                    list[a] = list[b];
                    list[b] = t;
                }
            }
        for (int k = 0; k < n; k++)
        {
            int i = list[k];
            int sy = oam[i * 4] - 16, sx = oam[i * 4 + 1] - 8;
            u8 t = oam[i * 4 + 2], at = oam[i * 4 + 3];
            int row = ly - sy;
            if (at & 0x40)
                row = h - 1 - row;
            if (h == 16)
                t &= 0xFE;
            u16 addr = t * 16 + row * 2;
            u8 lo = vram[addr], hi = vram[addr + 1];
            u8 pal = io[(at & 0x10) ? 0x49 : 0x48];
            for (int p = 0; p < 8; p++)
            {
                int x = sx + p;
                if (x < 0 || x >= 160)
                    continue;
                int bit = (at & 0x20) ? p : 7 - p;
                int idx = (((hi >> bit) & 1) << 1) | ((lo >> bit) & 1);
                if (!idx)
                    continue;
                if ((at & 0x80) && bgcol[x])
                    continue;
                fb[ly * 160 + x] = palette[(pal >> (idx * 2)) & 3];
            }
        }
    }
}

static void update_stat(void)
{
    u8 s = io[0x41];
    int lyc = io[0x44] == io[0x45];
    io[0x41] = (s & 0x78) | (lyc ? 4 : 0) | ppu_mode | 0x80;
    int line = (ppu_mode == 0 && (s & 0x08)) || (ppu_mode == 1 && (s & 0x10)) ||
               (ppu_mode == 2 && (s & 0x20)) || (lyc && (s & 0x40));
    if (line && !stat_line)
        io[0x0F] |= 2;
    stat_line = line;
}

void ppu_tick(int cycles)
{
    if (!(io[0x40] & 0x80))
        return;
    dots += cycles;
    int ly = io[0x44], prev = ppu_mode;
    if (dots >= 456)
    {
        dots -= 456;
        ly++;
        if (ly == 144)
        {
            io[0x0F] |= 1;
            frame_ready = 1;
        }
        if (ly > 153)
        {
            ly = 0;
            win_line = 0;
        }
        io[0x44] = ly;
    }
    int m = ly >= 144 ? 1 : (dots < 80 ? 2 : (dots < 252 ? 3 : 0));
    if (m != prev)
    {
        ppu_mode = m;
        if (m == 0)
            render_line(ly);
    }
    update_stat();
}

void gpu_reset(void) { ppu_mode = 1; }
