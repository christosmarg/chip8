#define _DEFAULT_SOURCE
#include <SDL2/SDL.h>
#include <unistd.h>
#include "chip8.h"

static const uint8_t keymap[16] = {
    SDLK_1, SDLK_2, SDLK_3, SDLK_4,
    SDLK_q, SDLK_w, SDLK_e, SDLK_r,
    SDLK_a, SDLK_s, SDLK_d, SDLK_f,
    SDLK_z, SDLK_x, SDLK_c, SDLK_v
};

static int
evts(Chip8 *chip8)
{
    int i;
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT || e.key.keysym.sym == SDLK_ESCAPE) return FALSE; 
        if (e.type == SDL_KEYDOWN)
            for (i = 0; i < 16; i++)
                if (e.key.keysym.sym == keymap[i])
                    chip8->keys[i] = TRUE;
        if (e.type == SDL_KEYUP)
            for (i = 0; i < 16; i++)
                if (e.key.keysym.sym == keymap[i])
                    chip8->keys[i] = FALSE;
    }
    return TRUE;
}

static void
render(SDL_Renderer *ren, SDL_Texture *tex, Chip8 *chip8)
{
    int i;
    uint32_t pixels[2048];
    chip8->drawflag = FALSE;
    for (i = 0; i < 2048; i++)
    {
        uint8_t pixel = chip8->gfx[i];
        pixels[i] = (0x00FFFFFF * pixel) | 0xFF000000;
    }
    SDL_UpdateTexture(tex, NULL, pixels, 64 * sizeof(Uint32));
    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, NULL, NULL);
    SDL_RenderPresent(ren);
}

int
main(int argc, char **argv)
{
    int w = 1024, h = 512;
    srand(time(NULL));

    if (argc != 2)
    {
        fprintf(stderr, "Usage: ./chip8 [ROM]\n");
        return EXIT_FAILURE;
    }
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
        fprintf(stderr, "Cannot initialize SDL. Exiting. . .\n");
        return EXIT_FAILURE;
    }
    SDL_Window *win = SDL_CreateWindow("CHIP-8 Emulator", SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED, w, h,
                            SDL_WINDOW_SHOWN);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, 0);
    SDL_RenderSetLogicalSize(ren, w, h);
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING, 64, 32);
    if (!win || !ren || !tex)
    {
        fprintf(stderr, "SDL error. Exiting. . .\n%s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    Chip8 chip8;
    chip8_init(&chip8);
    if (!chip8_rom_load(&chip8, argv[1])) return EXIT_FAILURE;
    for (; evts(&chip8); usleep(1500))
    {
        chip8_emulate(&chip8);
        if (chip8.drawflag) render(ren, tex, &chip8);
    }

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return EXIT_SUCCESS;
}
