#ifndef CHIP8_H
#define CHIP8_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
	uint8_t memory[4096];
	uint16_t stack[16];
	uint16_t sp;

	uint8_t V[16];
	uint16_t opcode;
	uint16_t I;
	uint16_t pc;

	uint8_t delaytimer;
	uint8_t soundtimer;
	uint8_t gfx[64 * 32];
	uint8_t keys[16];
	int drawflag;
} Chip8;

extern Chip8 chip8;

void chip8_init(Chip8 *chip8);
int load(Chip8 *chip8, const char *fpath);
void emulate(Chip8 *chip8);
void fetch(Chip8 *chip8);
int decode(Chip8 *chip8);
void execute(Chip8 *chip8);
void update_timers(Chip8 *chip8);

#endif /* CHIP8_H */
