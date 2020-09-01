#ifndef CHIP8_H
#define CHIP8_H

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TRUE  1
#define FALSE 0

typedef struct {
    uint8_t   memory[4096];
    uint8_t   V[16];
    uint8_t   gfx[64 * 32];
    uint8_t   keys[16];
    uint8_t   delaytimer;
    uint8_t   soundtimer;
    uint8_t   drawflag;
    uint16_t  stack[16];
    uint16_t  sp;
    uint16_t  opcode;
    uint16_t  I;
    uint16_t  pc;
} Chip8;

extern Chip8 chip8;

extern void  chip8_init(Chip8 *chip8);
extern int   chip8_rom_load(Chip8 *chip8, const char *fpath);
extern void  chip8_emulate(Chip8 *chip8);
extern int   chip8_decode(Chip8 *chip8);
extern void  chip8_timers_update(Chip8 *chip8);

#endif /* CHIP8_H */
