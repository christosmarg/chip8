/* See LICENSE file for copyright and license details. */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <SDL2/SDL.h>

#ifdef _WIN_32
typedef unsigned int   uint32_t
typedef unsigned short uint16_t
typedef unsigned char  uint8_t
#else
#include <inttypes.h>
#endif /* _WIN_32 */

#define VX_MASK(x)   ((x & 0x0f00) >> 8)
#define VY_MASK(x)   ((x & 0x00f0) >> 4)
#define NN_MASK(x)   (x & 0x00ff)
#define NNN_MASK(x)  (x & 0x0fff)
#define EXECUTE(pc)  do { pc += 2; } while (0)
#define ROM_SIZE_MAX (4096 - 512)

struct Chip8 {
        uint16_t  I;
        uint16_t  opcode;
        uint16_t  pc;
        uint16_t  sp;
        uint16_t  stack[16];
        uint8_t   delaytimer;
        uint8_t   drawflag;
        uint8_t   gfx[64 * 32];
        uint8_t   keys[16];
        uint8_t   memory[4096];
        uint8_t   soundtimer;
        uint8_t   V[16];
};

static const uint8_t keymap[16] = {
        SDLK_1, SDLK_2, SDLK_3, SDLK_4,
        SDLK_q, SDLK_w, SDLK_e, SDLK_r,
        SDLK_a, SDLK_s, SDLK_d, SDLK_f,
        SDLK_z, SDLK_x, SDLK_c, SDLK_v
};

static void  chip8_init(struct Chip8 *);
static void  romload(struct Chip8 *, const char *);
static void  emulate(struct Chip8 *);
static int   decode(struct Chip8 *);
static void  timers_update(struct Chip8 *);
static int   evts(struct Chip8 *);
static void  render(SDL_Renderer *, SDL_Texture *, struct Chip8 *);
static void  die(const char *, ...);

#define I           chip8->I
#define opcode      chip8->opcode
#define pc          chip8->pc
#define sp          chip8->sp
#define stack       chip8->stack
#define delaytimer  chip8->delaytimer
#define drawflag    chip8->drawflag
#define gfx         chip8->gfx
#define keys        chip8->keys
#define memory      chip8->memory
#define soundtimer  chip8->soundtimer
#define V           chip8->V

void
chip8_init(struct Chip8 *chip8)
{
        int i;
        uint8_t fontset[80] = {
                0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
                0x20, 0x60, 0x20, 0x20, 0x70, // 1
                0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
                0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
                0x90, 0x90, 0xF0, 0x10, 0x10, // 4
                0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
                0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
                0xF0, 0x10, 0x20, 0x40, 0x40, // 7
                0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
                0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
                0xF0, 0x90, 0xF0, 0x90, 0x90, // A
                0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
                0xF0, 0x80, 0x80, 0x80, 0xF0, // C
                0xE0, 0x90, 0x90, 0x90, 0xE0, // D
                0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
                0xF0, 0x80, 0xF0, 0x80, 0x80  // F
        };
        pc         = 0x200;
        opcode     = 0;
        I          = 0;
        sp         = 0;
        delaytimer = 0;
        soundtimer = 0;
        memset(V,      0, 16   * sizeof(uint8_t));
        memset(keys,   0, 16   * sizeof(uint8_t));
        memset(stack,  0, 16   * sizeof(uint16_t));
        memset(gfx,    0, 2048 * sizeof(uint8_t));
        memset(memory, 0, 4096 * sizeof(uint8_t));
        for (i = 0; i < 80; memory[i] = fontset[i], i++)
                ;
}

void
romload(struct Chip8 *chip8, const char *fpath)
{
        FILE *rom;
        long romsize;
        size_t res;
        int i;
        char *buf;

        if ((rom = fopen(fpath, "rb")) == NULL)
                die("error loading ROM (%s).", fpath);
        fseek(rom, 0, SEEK_END);
        romsize = ftell(rom);
        rewind(rom);

        if ((buf = malloc(romsize * sizeof(char))) == NULL)
                die("cannot allocate memory.");
        if ((res = fread(buf, sizeof(char), (size_t)romsize, rom)) != romsize)
                die("error reading ROM.");
        if (romsize < ROM_SIZE_MAX)
                for (i = 0; i < romsize; i++)
                        memory[i + 512] = (uint8_t)buf[i];
        else
                die("ROM cannot fit into memory.");

        fclose(rom);
        free(buf);
}

void
emulate(struct Chip8 *chip8)
{
        opcode = memory[pc] << 8 | memory[pc + 1]; // fetch
        if (decode(chip8)) {
                EXECUTE(pc); // execute
                timers_update(chip8);
        }
        printf("Opcode: %x\tMemory: %x\tI: %x\tSP: %x\tPC: %d\n",
                opcode, memory[pc] << 8 | memory[pc + 1], I, sp, pc);
}

int
decode(struct Chip8 *chip8)
{
        int i;

        switch (opcode & 0xF000) {
        case 0x0000: // 00E_
                switch (NN_MASK(opcode)) {
                case 0xE0: // 00E0 - Clear screen
                        memset(gfx, 0, 2048 * sizeof(uint8_t));
                        drawflag = 1;
                        break;
                case 0xEE: // 00EE - Return from subroutine
                        pc = stack[--sp];
                        break;
                default:
                        fprintf(stderr, "Unknown opcode: %x\n", opcode);
                        return 0;
                }
                break;
        case 0x1000: // 1NNN - Jump to address NNN
                pc = NNN_MASK(opcode) - 2;
                break;
        case 0x2000: // 2NNN - Call subroutine at NNN
                stack[sp++] = pc;
                pc = NNN_MASK(opcode) - 2;
        case 0x3000: // 3NNN - Skip next instruction if VX == NN
                if (V[VX_MASK(opcode)] == NN_MASK(opcode))
                        EXECUTE(pc);
                break;
        case 0x4000: // 4NNN - Skip next instruction if VX != NN
                if (V[VX_MASK(opcode)] != NN_MASK(opcode))
                        EXECUTE(pc);
                break;
        case 0x5000: // 5XY0 - Skip next instruction if VX == VY
                if (V[VX_MASK(opcode)] == V[VY_MASK(opcode)])
                        EXECUTE(pc);
                break;
        case 0x6000: // 6XNN - Set VX to NN
                V[VX_MASK(opcode)] = NN_MASK(opcode);
                break;
        case 0x7000: // 7XNN - Add NN to VX
                V[VX_MASK(opcode)] += NN_MASK(opcode);
                break;
        case 0x8000: // 8XY_
                switch (opcode & 0x000F) {
                case 0x0000: // 8XY0 - Set VX to VY
                        V[VX_MASK(opcode)] = V[VY_MASK(opcode)];
                        break;
                case 0x0001: // 8XY1 - Set VX to (VX OR VY)
                        V[VX_MASK(opcode)] |= V[VY_MASK(opcode)];
                        break;
                case 0x0002: // 8XY2 - Set VX to (VX AND VY)
                        V[VX_MASK(opcode)] &= V[VY_MASK(opcode)];
                        break;
                case 0x0003: // 8XY3 - Set VX to (VX XOR VY)
                        V[VX_MASK(opcode)] ^= V[VY_MASK(opcode)];
                        break;
                case 0x0004: // 8XY4 - Add VY to VX, VF = 1 if there is a carry
                        V[VX_MASK(opcode)] += V[VY_MASK(opcode)];
                        V[0xF] = (V[VY_MASK(opcode)] > (0xFF - V[VX_MASK(opcode)])) ? 1 : 0;
                        break;
                case 0x0005: // 8XY5 - Sub VY from VX, VF = 0 if there is a borrow
                        V[0xF] = (V[VY_MASK(opcode)] > V[VX_MASK(opcode)]) ? 0 : 1;
                        V[VX_MASK(opcode)] -= V[VY_MASK(opcode)];
                        break;
                case 0x0006: // 8XY6 - Shift VX right by 1. VF = LSB of VX before shift
                        V[0xF] = V[VX_MASK(opcode)] & 0x1;
                        V[VX_MASK(opcode)] >>= 1;
                        break;
                case 0x0007: // 8XY7 - Set VX to VY-VX. VF = 0 if there is a borrow
                        V[0xF] = (V[VX_MASK(opcode)] > V[VY_MASK(opcode)]) ? 0 : 1;
                        V[VX_MASK(opcode)] = V[VY_MASK(opcode)] - V[VX_MASK(opcode)];
                        break;
                case 0x000E: // 8XYE - Shift VX left by 1. VF = MSB of VX before shift
                        V[0xF] = V[VX_MASK(opcode)] >> 7;
                        V[VX_MASK(opcode)] <<= 1;
                        break;
                default:
                        fprintf(stderr, "Unknown opcode: %x\n", opcode);
                        return 0;
                }
                break;
        case 0x9000: // 9XY0 - Skip next instruction if VX != VY
                if (V[VX_MASK(opcode)] != V[VY_MASK(opcode)])
                        EXECUTE(pc);
                break;
        case 0xA000: // ANNN - Set I to the address NNN
                I = NNN_MASK(opcode);
                break;
        case 0xB000: // BNNN - Jump to NNN + V0
                pc = ((NNN_MASK(opcode)) + V[0]) - 2;
                break;
        case 0xC000: // CNNN - Set VX to random number masked by NN
                V[VX_MASK(opcode)] = (rand() % (0xFF + 1)) & (NN_MASK(opcode));
                break;
        case 0xD000: { // Draw an 8 pixel sprite at (VX, VY)
                int yl, xl;
                uint16_t h = opcode & 0x000F;
                uint16_t pixel;
                uint8_t VX = V[VX_MASK(opcode)];
                uint8_t VY = V[VY_MASK(opcode)];

                V[0xF] = 0;
                for (yl = 0; yl < h; yl++) {
                        pixel = memory[I + yl];
                        for (xl = 0; xl < 8; xl++) {
                                if ((pixel & (0x80 >> xl)) != 0) {
                                        if (gfx[VX + xl + ((VY + yl) * 64)] == 1)
                                                V[0xF] = 1;
                                        gfx[VX + xl + ((VY + yl) * 64)] ^= 1;
                                }
                        }
                }
                drawflag = 1;
        }
                break;
        case 0xE000: // EX__
                switch (NN_MASK(opcode)) {
                case 0x009E: // EX9E - Skip next instruction if key in VX is pressed
                        if (keys[V[VX_MASK(opcode)]])
                                EXECUTE(pc);
                        break;
                case 0x00A1: // EXA1 - Skip next instruction if key in VX isn't pressed
                        if (!keys[V[VX_MASK(opcode)]])
                                EXECUTE(pc);
                        break;
                default:
                        fprintf(stderr, "Unknown opcode: %x\n", opcode);
                        return 0;
                }
                break;
        case 0xF000: // FX__
                switch (NN_MASK(opcode)) {
                case 0x0007: // FX07 - Set VX to delaytimer
                        V[VX_MASK(opcode)] = delaytimer;
                        break;
                case 0x000A: { // FX0A - Wait for key press and then store it in VX
                        int keypressed = 0;

                        for (i = 0; i < 16; i++) {
                                if (keys[i]) {
                                        V[VX_MASK(opcode)] = i;
                                        keypressed = 1;
                                }
                        }
                        if (!keypressed)
                                return 0;
                }
                        break;
                case 0x0015: // FX15 - Set the delaytimer to VX
                        delaytimer = V[VX_MASK(opcode)];
                        break;
                case 0x0018: // FX18 - Set the soundtimer to VX
                        soundtimer = V[VX_MASK(opcode)];
                        break;
                case 0x001E: // FX1E - Add VX to I
                        V[0xF] = ((I + V[VX_MASK(opcode)]) > 0xFFF) ? 1 : 0;
                        I += V[VX_MASK(opcode)];
                        break;
                case 0x0029: // FX29 - Set I to the location of the sprite for char VX
                        I = V[VX_MASK(opcode)] * 0x5;
                        break;
                case 0x0033: // FX33 - Store bin coded decimal of VX at I, I+1 and I+2
                        memory[I]   = V[VX_MASK(opcode)] / 100;
                        memory[I+1] = (V[VX_MASK(opcode)] / 10) % 10;
                        memory[I+2] = V[VX_MASK(opcode)] % 10;
                        break;
                case 0x0055: // FX55 - Store V0 to VX in memory starting at I
                        for (i = 0; i <= (VX_MASK(opcode)); i++)
                                memory[I + i] = V[i];
                        I += (VX_MASK(opcode)) + 1;
                        break;
                case 0x0065: // FX65 - Fill V0 to VX with vals from memory starting at I
                        for (i = 0; i <= (VX_MASK(opcode)); i++)
                                V[i] = memory[I + i];
                        I += (VX_MASK(opcode)) + 1;
                        break;
                default:
                        fprintf(stderr, "Unknown opcode: %x\n", opcode);
                        return 0;
                }
                break;
        default:
                fputs("Unimplemented opcode\n", stderr);
                return 0;
        }
        return 1;
}

void
timers_update(struct Chip8 *chip8)
{
        if (delaytimer > 0)
                delaytimer--;
        if (soundtimer > 0)
                soundtimer--;
}

#undef V
#undef pc
#undef opcode
#undef I
#undef sp
#undef memory
#undef gfx
#undef stack
#undef keys
#undef delaytimer
#undef soundtimer
#undef drawflag

int
evts(struct Chip8 *chip8)
{
        int i;
        SDL_Event e;

        while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT || e.key.keysym.sym == SDLK_ESCAPE)
                        return 0; 
                if (e.type == SDL_KEYDOWN)
                        for (i = 0; i < 16; i++)
                                if (e.key.keysym.sym == keymap[i])
                                        chip8->keys[i] = 1;
                if (e.type == SDL_KEYUP)
                        for (i = 0; i < 16; i++)
                                if (e.key.keysym.sym == keymap[i])
                                        chip8->keys[i] = 0;
        }
        return 1;
}

void
render(SDL_Renderer *ren, SDL_Texture *tex, struct Chip8 *chip8)
{
        int i;
        uint32_t pixels[2048];
        uint8_t pixel;

        chip8->drawflag = 0;
        for (i = 0; i < 2048; i++) {
                pixel = chip8->gfx[i];
                pixels[i] = (0x00FFFFFF * pixel) | 0xFF000000;
        }
        SDL_UpdateTexture(tex, NULL, pixels, 64 * sizeof(Uint32));
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
}

void
die(const char *fmt, ...)
{
        va_list args;
        
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);

        if (fmt[0] && fmt[strlen(fmt)-1] == ':') {
                fputc(' ', stderr);
                perror(NULL);
        } else
                fputc('\n', stderr);

        exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
        struct Chip8 chip8;
        int w = 1024, h = 512;

        srand(time(NULL));
        if (argc != 2)
                die("usage: %s [ROM]", argv[0]);
        if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
                die("cannot initalize SDL: %s", SDL_GetError());

        SDL_Window *win = SDL_CreateWindow("CHIP-8 Emulator", SDL_WINDOWPOS_UNDEFINED,
                                SDL_WINDOWPOS_UNDEFINED, w, h,
                                SDL_WINDOW_SHOWN);
        SDL_Renderer *ren = SDL_CreateRenderer(win, -1, 0);
        SDL_RenderSetLogicalSize(ren, w, h);
        SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
                SDL_TEXTUREACCESS_STREAMING, 64, 32);

        if (!win || !ren || !tex)
                die("SDL error: %s", SDL_GetError());

        chip8_init(&chip8);
        romload(&chip8, argv[1]);

        while (evts(&chip8)) {
                emulate(&chip8);
                if (chip8.drawflag)
                        render(ren, tex, &chip8);
                usleep(1500);
        }

        SDL_DestroyTexture(tex);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return EXIT_SUCCESS;
}
