#define _DEFAULT_SOURCE
#include "chip8.h"
#include <SDL2/SDL.h>
#include <unistd.h>

static const uint8_t keymap[16] = {
    SDLK_1, SDLK_2,
    SDLK_3, SDLK_4,
    SDLK_q, SDLK_w,
    SDLK_e, SDLK_r,
    SDLK_a, SDLK_s,
    SDLK_d, SDLK_f,
    SDLK_z, SDLK_x,
    SDLK_c, SDLK_v,
};

static int handle_events(Chip8 *chip8);
static void render(SDL_Renderer *r, SDL_Texture *t, Chip8 *chip8);

int
handle_events(Chip8 *chip8)
{
    int i;
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT) return FALSE; 
        if (e.type == SDL_KEYDOWN)
        {
            if (e.key.keysym.sym == SDLK_ESCAPE) return FALSE;
            for (i = 0; i < 16; i++)
                if (e.key.keysym.sym == keymap[i])
                    chip8->keys[i] = TRUE;
        }

        if (e.type == SDL_KEYUP)
            for (i = 0; i < 16; i++)
                if (e.key.keysym.sym == keymap[i])
                    chip8->keys[i] = FALSE;
    }
    return TRUE;
}

void
render(SDL_Renderer *r, SDL_Texture *t, Chip8 *chip8)
{
    int i;
    uint32_t pixels[2048];
    chip8->drawflag = FALSE;
    for (i = 0; i < 2048; i++)
    {
        uint8_t pixel = chip8->gfx[i];
        pixels[i] = (0x00FFFFFF * pixel) | 0xFF000000;
    }
    SDL_UpdateTexture(t, NULL, pixels, 64 * sizeof(Uint32));
    SDL_RenderClear(r);
    SDL_RenderCopy(r, t, NULL, NULL);
    SDL_RenderPresent(r);
}

int
main(int argc, char **argv)
{
    SDL_Window *win = NULL;
    int w = 1024, h = 512;
    srand(time(NULL));

    if (argc != 2)
    {
        ERROR("%s", "Usage: ./chip8 [ROM]");
        return EXIT_FAILURE;
    }
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
        ERROR("%s", "Cannot initialize SDL. Exiting. . .");
        return EXIT_FAILURE;
    }

    win = SDL_CreateWindow("CHIP-8 Emulator", SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED, w, h,
                            SDL_WINDOW_SHOWN);
    if (win == NULL)
    {
        ERROR("Cannot create SDL window. Exiting. . .\n%s", SDL_GetError());
        return EXIT_FAILURE;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(win, -1, 0);
    SDL_RenderSetLogicalSize(renderer, w, h);
    SDL_Texture *texture = SDL_CreateTexture(
            renderer, SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING, 64, 32);

    Chip8 chip8;
    chip8_init(&chip8);
    if (!load(&chip8, argv[1])) return EXIT_FAILURE;
    for (;;)
    {
        emulate(&chip8);
        if (!handle_events(&chip8)) return EXIT_SUCCESS;
        if (chip8.drawflag) render(renderer, texture, &chip8);
        usleep(1500);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return EXIT_SUCCESS;
}
