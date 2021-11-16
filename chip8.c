/* See LICENSE file for copyright and license details. */
#include <sys/types.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <SDL2/SDL.h>

#define EXECUTE(pc) do { pc += 2; } while (0)

struct chip8 {
	uint16_t stack[16];
	uint16_t i;
	uint16_t opcode;
	uint16_t pc;
	uint16_t sp;
	uint8_t mem[4096];
	uint8_t gfx[64 * 32];
	uint8_t v[16];
	uint8_t keys[16];
	uint8_t delaytimer;
	uint8_t soundtimer;
	uint8_t drawflag;
};

static void chip8_init(struct chip8 *);
static void rom_load(struct chip8 *, const char *);
static void emulate(struct chip8 *);
static int decode(struct chip8 *);
static int handle_events(struct chip8 *);
static void render(SDL_Renderer *, SDL_Texture *, struct chip8 *);

static void
chip8_init(struct chip8 *c8)
{
	int i;
	uint8_t fontset[80] = {
		0xf0, 0x90, 0x90, 0x90, 0xf0, // 0
		0x20, 0x60, 0x20, 0x20, 0x70, // 1
		0xf0, 0x10, 0xf0, 0x80, 0xf0, // 2
		0xf0, 0x10, 0xf0, 0x10, 0xf0, // 3
		0x90, 0x90, 0xf0, 0x10, 0x10, // 4
		0xf0, 0x80, 0xf0, 0x10, 0xf0, // 5
		0xf0, 0x80, 0xf0, 0x90, 0xf0, // 6
		0xf0, 0x10, 0x20, 0x40, 0x40, // 7
		0xf0, 0x90, 0xf0, 0x90, 0xf0, // 8
		0xf0, 0x90, 0xf0, 0x10, 0xf0, // 9
		0xf0, 0x90, 0xf0, 0x90, 0x90, // a
		0xe0, 0x90, 0xe0, 0x90, 0xe0, // b
		0xf0, 0x80, 0x80, 0x80, 0xf0, // c
		0xe0, 0x90, 0x90, 0x90, 0xe0, // d
		0xf0, 0x80, 0xf0, 0x80, 0xf0, // e
		0xf0, 0x80, 0xf0, 0x80, 0x80  // f
	};

	c8->pc = 0x200;
	c8->opcode = 0;
	c8->i = 0;
	c8->sp = 0;
	c8->delaytimer = 0;
	c8->soundtimer = 0;
	memset(c8->v, 0, sizeof(c8->v));
	memset(c8->keys, 0, sizeof(c8->keys));
	memset(c8->stack, 0, sizeof(c8->stack));
	memset(c8->gfx, 0, sizeof(c8->gfx));
	memset(c8->mem, 0, sizeof(c8->mem));
	for (i = 0; i < 80; i++)
		c8->mem[i] = fontset[i];
}

static void
rom_load(struct chip8 *c8, const char *fpath)
{
	FILE *rom;
	char *buf;
	size_t res, romsiz;
	int i;

	if ((rom = fopen(fpath, "rb")) == NULL)
		err(1, "fopen: %s", fpath);
	fseek(rom, 0, SEEK_END);
	if ((romsiz = ftell(rom)) >= 4092 - 512)
		errx(1, "ROM cannot fit into memory");
	rewind(rom);
	if ((buf = malloc(romsiz + 1)) == NULL)
		err(1, "malloc");
	if ((res = fread(buf, sizeof(char), romsiz, rom)) != romsiz)
		err(1, "fread");
	buf[romsiz] = '\0';
	for (i = 0; i < romsiz; i++)
		c8->mem[i + 512] = buf[i];
	fclose(rom);
	free(buf);
}

static void
emulate(struct chip8 *c8)
{
	uint16_t *pc = &c8->pc;

	c8->opcode = c8->mem[*pc] << 8 | c8->mem[*pc + 1];
	if (decode(c8)) {
		EXECUTE(*pc);
		if (c8->delaytimer > 0)
			c8->delaytimer--;
		if (c8->soundtimer > 0)
			c8->soundtimer--;
	}
	printf("Opcode: %x\tMemory: %x\tI: %x\tSP: %x\tPC: %d\n",
	    c8->opcode, c8->mem[*pc] << 8 | c8->mem[*pc + 1], c8->i, c8->sp, *pc);
}

static int
decode(struct chip8 *c8)
{
	int yl, xl, pos, i, keypress = 0;
	uint16_t vx, vy, nn, nnn;
	uint16_t h, pixel;
	uint8_t cvx, cvy;

	vx = (c8->opcode & 0x0f00) >> 8;
	vy = (c8->opcode & 0x00f0) >> 4;
	nn = c8->opcode & 0x00ff;
	nnn = c8->opcode & 0x0fff;

	switch (c8->opcode & 0xf000) {
	case 0x0000: // 00E_
		switch (nn) {
		case 0xe0: // 00E0 - Clear screen
			memset(c8->gfx, 0, sizeof(c8->gfx));
			c8->drawflag = 1;
			break;
		case 0xee: // 00EE - Return from subroutine
			c8->pc = c8->stack[--c8->sp];
			break;
		default:
			warnx("unknown opcode: %x\n", c8->opcode);
			return (0);
		}
		break;
	case 0x1000: // 1NNN - Jump to address NNN
		c8->pc = nnn - 2;
		break;
	case 0x2000: // 2NNN - Call subroutine at NNN
		c8->stack[c8->sp++] = c8->pc;
		c8->pc = nnn - 2;
	case 0x3000: // 3NNN - Skip next instruction if VX == NN
		if (c8->v[vx] == nn)
			EXECUTE(c8->pc);
		break;
	case 0x4000: // 4NNN - Skip next instruction if VX != NN
		if (c8->v[vx] != nn)
			EXECUTE(c8->pc);
		break;
	case 0x5000: // 5XY0 - Skip next instruction if VX == VY
		if (c8->v[vx] == c8->v[vy])
			EXECUTE(c8->pc);
		break;
	case 0x6000: // 6XNN - Set VX to NN
		c8->v[vx] = nn;
		break;
	case 0x7000: // 7XNN - Add NN to VX
		c8->v[vx] += nn;
		break;
	case 0x8000: // 8XY_
		switch (c8->opcode & 0x000f) {
		case 0x0000: // 8XY0 - Set VX to VY
			c8->v[vx] = c8->v[vy];
			break;
		case 0x0001: // 8XY1 - Set VX to (VX OR VY)
			c8->v[vx] |= c8->v[vy];
			break;
		case 0x0002: // 8XY2 - Set VX to (VX AND VY)
			c8->v[vx] &= c8->v[vy];
			break;
		case 0x0003: // 8XY3 - Set VX to (VX XOR VY)
			c8->v[vx] ^= c8->v[vy];
			break;
		case 0x0004: // 8XY4 - Add VY to VX, VF = 1 if there is a carry
			c8->v[vx] += c8->v[vy];
			c8->v[0xf] = (c8->v[vy] > (0xff - c8->v[vx])) ? 1 : 0;
			break;
		case 0x0005: // 8XY5 - Sub VY from VX, VF = 0 if there is a borrow
			c8->v[0xf] = (c8->v[vy] > c8->v[vx]) ? 0 : 1;
			c8->v[vx] -= c8->v[vy];
			break;
		case 0x0006: // 8XY6 - Shift VX right by 1. VF = LSB of VX before shift
			c8->v[0xf] = c8->v[vx] & 0x1;
			c8->v[vx] >>= 1;
			break;
		case 0x0007: // 8XY7 - Set VX to VY-VX. VF = 0 if there is a borrow
			c8->v[0xf] = (c8->v[vx] > c8->v[vy]) ? 0 : 1;
			c8->v[vx] = c8->v[vy] - c8->v[vx];
			break;
		case 0x000e: // 8XYE - Shift VX left by 1. VF = MSB of VX before shift
			c8->v[0xf] = c8->v[vx] >> 7;
			c8->v[vx] <<= 1;
			break;
		default:
			warnx("unknown opcode: %x\n", c8->opcode);
			return (0);
		}
		break;
	case 0x9000: // 9XY0 - Skip next instruction if VX != VY
		if (c8->v[vx] != c8->v[vy])
			EXECUTE(c8->pc);
		break;
	case 0xa000: // ANNN - Set I to the address NNN
		c8->i = nnn;
		break;
	case 0xb000: // BNNN - Jump to NNN + V0
		c8->pc = (nnn + c8->v[0]) - 2;
		break;
	case 0xc000: // CNNN - Set VX to random number masked by NN
		c8->v[vx] = (rand() % (0xff + 1)) & nn;
		break;
	case 0xd000: // Draw an 8 pixel sprite at (VX, VY)
		cvx = c8->v[vx];
		cvy = c8->v[vy];
		c8->v[0xf] = 0;
		h = c8->opcode & 0x000f;
		for (yl = 0; yl < h; yl++) {
			for (xl = 0; xl < 8; xl++) {
				pixel = c8->mem[c8->i + yl];
				pos = cvx + xl + ((cvy + yl) * 64);
				if ((pixel & (0x80 >> xl)) == 0)
					continue;
				if (c8->gfx[pos] == 1)
					c8->v[0xf] = 1;
				c8->gfx[pos] ^= 1;
			}
		}
		c8->drawflag = 1;
		break;
	case 0xe000: // EX__
		switch (nn) {
		case 0x009e: // EX9E - Skip next instruction if key in VX is pressed
			if (c8->keys[c8->v[vx]])
				EXECUTE(c8->pc);
			break;
		case 0x00a1: // EXA1 - Skip next instruction if key in VX isn't pressed
			if (!c8->keys[c8->v[vx]])
				EXECUTE(c8->pc);
			break;
		default:
			warnx("unknown opcode: %x\n", c8->opcode);
			return (0);
		}
		break;
	case 0xf000: // FX__
		switch (nn) {
		case 0x0007: // FX07 - Set VX to delaytimer
			c8->v[vx] = c8->delaytimer;
			break;
		case 0x000a: // FX0A - Wait for key press and then store it in VX
			for (i = 0; i < 16; i++) {
				if (c8->keys[i]) {
					c8->v[vx] = i;
					keypress = 1;
				}
			}
			if (!keypress)
				return (0);
			break;
		case 0x0015: // FX15 - Set the delaytimer to VX
			c8->delaytimer = c8->v[vx];
			break;
		case 0x0018: // FX18 - Set the soundtimer to VX
			c8->soundtimer = c8->v[vx];
			break;
		case 0x001e: // FX1E - Add VX to I
			c8->v[0xf] = ((c8->i + c8->v[vx]) > 0xfff) ? 1 : 0;
			c8->i += c8->v[vx];
			break;
		case 0x0029: // FX29 - Set I to the location of the sprite for char VX
			c8->i = c8->v[vx] * 0x5;
			break;
		case 0x0033: // FX33 - Store bin coded decimal of VX at I, I+1 and I+2
			c8->mem[c8->i] = c8->v[vx] / 100;
			c8->mem[c8->i+1] = (c8->v[vx] / 10) % 10;
			c8->mem[c8->i+2] = c8->v[vx] % 10;
			break;
		case 0x0055: // FX55 - Store V0 to VX in memory starting at I
			for (i = 0; i <= (vx); i++)
				c8->mem[c8->i + i] = c8->v[i];
			c8->i += (vx) + 1;
			break;
		case 0x0065: // FX65 - Fill V0 to VX with vals from memory starting at I
			for (i = 0; i <= (vx); i++)
				c8->v[i] = c8->mem[c8->i + i];
			c8->i += vx + 1;
			break;
		default:
			warnx("unknown opcode: %x\n", c8->opcode);
			return (0);
		}
		break;
	default:
		warnx("unimplemented opcode\n");
		return (0);
	}
	return (1);
}

static int
handle_events(struct chip8 *c8)
{
	SDL_Event e;
	int i;
	static const uint8_t keymap[16] = {
		SDLK_1, SDLK_2, SDLK_3, SDLK_4,
		SDLK_q, SDLK_w, SDLK_e, SDLK_r,
		SDLK_a, SDLK_s, SDLK_d, SDLK_f,
		SDLK_z, SDLK_x, SDLK_c, SDLK_v
	};

	while (SDL_PollEvent(&e)) {
		if (e.type == SDL_QUIT || e.key.keysym.sym == SDLK_ESCAPE)
			return (0); 
		if (e.type != SDL_KEYUP && e.type != SDL_KEYDOWN)
			continue;
		for (i = 0; i < 16; i++) {
			if (e.key.keysym.sym != keymap[i])
				continue;
			if (e.type == SDL_KEYUP)
				c8->keys[i] = 0;
			else if (e.type == SDL_KEYDOWN)
				c8->keys[i] = 1;
		}
	}
	return (1);
}

static void
render(SDL_Renderer *ren, SDL_Texture *tex, struct chip8 *c8)
{
	uint32_t pixels[2048];
	int i;

	c8->drawflag = 0;
	for (i = 0; i < 2048; i++)
		pixels[i] = (0x00ffffff * c8->gfx[i]) | 0xff000000;
	SDL_UpdateTexture(tex, NULL, pixels, 64 * sizeof(Uint32));
	SDL_RenderClear(ren);
	SDL_RenderCopy(ren, tex, NULL, NULL);
	SDL_RenderPresent(ren);
}

int
main(int argc, char *argv[])
{
	SDL_Window *win;
	SDL_Renderer *ren;
	SDL_Texture *tex;
	struct chip8 *c8;
	int w = 1024, h = 512;

	if (argc != 2) {
		fprintf(stderr, "usage: %s rom\n", *argv);
		return (1);
	}
	if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
		errx(1, "SDL_Init: %s", SDL_GetError());
	if ((c8 = malloc(sizeof(struct chip8))) == NULL)
		err(1, "malloc");

	chip8_init(c8);
	rom_load(c8, *++argv);
	srand(time(NULL));

	win = SDL_CreateWindow("CHIP-8", SDL_WINDOWPOS_UNDEFINED,
	    SDL_WINDOWPOS_UNDEFINED, w, h,
	    SDL_WINDOW_SHOWN);
	ren = SDL_CreateRenderer(win, -1, 0);
	SDL_RenderSetLogicalSize(ren, w, h);
	tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
	    SDL_TEXTUREACCESS_STREAMING, 64, 32);

	if (!win || !ren || !tex)
		errx(1, "SDL error: %s", SDL_GetError());
	for (;;) {
		if (!handle_events(c8))
			break;
		emulate(c8);
		if (c8->drawflag)
			render(ren, tex, c8);
		/* FIXME: VERY slow on some machines */
		usleep(1500);
	}

	free(c8);
	SDL_DestroyTexture(tex);
	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	SDL_Quit();

	return (0);
}
