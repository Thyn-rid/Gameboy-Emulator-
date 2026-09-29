/* main.c - window, input, frame loop */
#define SDL_MAIN_HANDLED /* keep a normal main() on Windows */
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "gb.h"

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: %s rom.gb [--test]\n", argv[0]);
        return 1;
    }
    int headless = argc > 2 && !strcmp(argv[2], "--test");

    if (!load_rom(argv[1]))
        return 1;
    mem_reset();
    cpu_reset();
    timer_reset();
    gpu_reset();

    SDL_Window *win = NULL;
    SDL_Renderer *ren = NULL;
    SDL_Texture *tex = NULL;
    if (!headless)
    {
        SDL_SetMainReady();
        if (SDL_Init(SDL_INIT_VIDEO) < 0)
        {
            fprintf(stderr, "SDL: %s\n", SDL_GetError());
            return 1;
        }
        win = SDL_CreateWindow("gb", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 576, 0);
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
        SDL_RenderSetLogicalSize(ren, 160, 144);
        tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 160, 144);
    }

    for (;;)
    {
        Uint32 t0 = headless ? 0 : SDL_GetTicks();
        int cyc = 0;
        frame_ready = 0;
        while (!frame_ready && cyc < 70224)
            cyc += cpu_step();
        if (headless)
            continue;

        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
                return 0;
            if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP)
            {
                int down = e.type == SDL_KEYDOWN;
                u8 m = 0;
                u8 *reg = &joy_btn;
                switch (e.key.keysym.sym)
                {
                case SDLK_ESCAPE:
                    return 0;
                case SDLK_RIGHT:
                    m = 1;
                    reg = &joy_dir;
                    break;
                case SDLK_LEFT:
                    m = 2;
                    reg = &joy_dir;
                    break;
                case SDLK_UP:
                    m = 4;
                    reg = &joy_dir;
                    break;
                case SDLK_DOWN:
                    m = 8;
                    reg = &joy_dir;
                    break;
                case SDLK_z:
                    m = 1;
                    break; /* A      */
                case SDLK_x:
                    m = 2;
                    break; /* B      */
                case SDLK_BACKSPACE:
                case SDLK_RSHIFT:
                    m = 4;
                    break; /* Select */
                case SDLK_RETURN:
                    m = 8;
                    break; /* Start  */
                }
                if (m)
                {
                    if (down)
                    {
                        *reg &= ~m;
                        io[0x0F] |= 0x10;
                    }
                    else
                        *reg |= m;
                }
            }
        }
        SDL_UpdateTexture(tex, NULL, fb, 160 * 4);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
        Uint32 el = SDL_GetTicks() - t0;
        if (el < 16)
            SDL_Delay(16 - el); /* ~60 fps */
    }
}
