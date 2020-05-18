#include "chip8.h"

#define V			chip8->V
#define pc			chip8->pc
#define opcode		chip8->opcode
#define I			chip8->I
#define sp			chip8->sp
#define memory		chip8->memory
#define gfx			chip8->gfx
#define stack		chip8->stack
#define keys		chip8->keys
#define delaytimer	chip8->delaytimer
#define soundtimer	chip8->soundtimer
#define drawflag	chip8->drawflag

void
chip8_init(Chip8 *chip8)
{
	uint8_t fontset[80] =
	{
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

	pc = 0x200;
	opcode = 0;
	I = 0;
	sp = 0;
	delaytimer = 0;
	soundtimer = 0;

	memset(V,		0, 16	* sizeof(uint8_t));
	memset(keys,	0, 16	* sizeof(uint8_t));
	memset(stack,	0, 16	* sizeof(uint16_t));
	memset(gfx,		0, 2048	* sizeof(uint8_t));
	memset(memory,	0, 4096	* sizeof(uint8_t));
	int i;
	for (i = 0; i < 80; i++)
		memory[i] = fontset[i];
}

int
load(Chip8 *chip8, const char *fpath)
{
	FILE *rom = fopen(fpath, "rb");
	if (rom == NULL)
	{
		fprintf(stderr, "Error loading ROM (%s). Exiting. . .\n", fpath);
		return -1;
	}
	fseek(rom, 0, SEEK_END);
	long romsize = ftell(rom);
	rewind(rom);

	char *buf = (char *)malloc(romsize * sizeof(char));
	if (buf == NULL)
	{
		fprintf(stderr, "Cannot allocate memory. Exiting. . .\n");
		return -1;
	}

	size_t res = fread(buf, sizeof(char), (size_t)romsize, rom);
	if (res != romsize)
	{
		fprintf(stderr, "Error reading ROM. Exiting. . .\n");
		return -1;
	}

	int i;
	if ((4096 - 512) > romsize)
		for (i = 0; i < romsize; i++)
			memory[i + 512] = (uint8_t)buf[i];
	else
	{
		fprintf(stderr, "ROM can't fit into memory. Exiting. . .\n");
		return -1;
	}

	fclose(rom);
	free(buf);
	return 1;
}

void
emulate(Chip8 *chip8)
{
	fetch(chip8);
	if (decode(chip8) > 0)
	{
		execute(chip8);
		update_timers(chip8);
	}

	printf("opcode: %x\tmemory: %x\tI: %x\tsp: %x\tpc: %d\n",
			opcode, memory[pc] << 8 | memory[pc + 1], I, sp, pc);
}

void
fetch(Chip8 *chip8)
{
	opcode = memory[pc] << 8 | memory[pc + 1];
}

int
decode(Chip8 *chip8)
{
	int i;
	switch (opcode & 0xF000)
	{
		case 0x0000: // 00E_
			switch (opcode & 0x00FF)
			{
				case 0xE0: // 00E0 - Clear screen
					memset(gfx, 0, 2048 * sizeof(uint8_t));
					drawflag = 1;
					break;
				case 0xEE: // 00EE - Return from subroutine
					pc = stack[--sp];
					break;
				default:
					fprintf(stderr, "Unknown opcode: %x\n", opcode);
					return -1;
			}
			break;
		case 0x1000: // 1NNN - Jump to address NNN
			pc = (opcode & 0x0FFF) - 2;
			break;
		case 0x2000: // 2NNN - Call subroutine at NNN
			stack[sp++] = pc;
			pc = (opcode & 0x0FFF) - 2;
		case 0x3000: // 3NNN - Skip next instruction if VX == NN
			if (V[(opcode & 0x0F00) >> 8] == (opcode & 0x00FF)) pc += 2;
			break;
		case 0x4000: // 4NNN - Skip next instruction if VX != NN
			if (V[(opcode & 0x0F00) >> 8] != (opcode & 0x00FF)) pc += 2;
			break;
		case 0x5000: // 5XY0 - Skip next instruction if VX == VY
			if (V[(opcode & 0x0F00) >> 8] == V[(opcode & 0x00F0) >> 4]) pc += 2;
			break;
		case 0x6000: // 6XNN - Set VX to NN
			V[(opcode & 0x0F00) >> 8] = opcode & 0x00FF;
			break;
		case 0x7000: // 7XNN - Add NN to VX
			V[(opcode & 0x0F00) >> 8] += opcode & 0x00FF;
			break;
		case 0x8000: // 8XY_
			switch (opcode & 0x000F)
			{
				case 0x0000: // 8XY0 - Set VX to VY
					V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x00F0) >> 4];
					break;
				case 0x0001: // 8XY1 - Set VX to (VX OR VY)
					V[(opcode & 0x0F00) >> 8] |= V[(opcode & 0x00F0) >> 4];
					break;
				case 0x0002: // 8XY2 - Set VX to (VX AND VY)
					V[(opcode & 0x0F00) >> 8] &= V[(opcode & 0x00F0) >> 4];
					break;
				case 0x0003: // 8XY3 - Set VX to (VX XOR VY)
					V[(opcode & 0x0F00) >> 8] ^= V[(opcode & 0x00F0) >> 4];
					break;
				case 0x0004: // 8XY4 - Add VY to VX, VF = 1 if there is a carry
					V[(opcode & 0x0F00) >> 8] += V[(opcode & 0x00F0) >> 4];
					V[0xF] = (V[(opcode & 0x00F0) >> 4] > (0xFF - V[(opcode & 0x0F00) >> 8])) ? 1 : 0;
					break;
				case 0x0005: // 8XY5 - Sub VY from VX, VF = 0 if there is a borrow
					V[0xF] = (V[(opcode & 0x00F0) >> 4] > V[(opcode & 0x0F00) >> 8]) ? 0 : 1;
					V[(opcode & 0x0F00) >> 8] -= V[(opcode & 0x00F0) >> 4];
					break;
				case 0x0006: // 8XY6 - Shift VX right by 1. VF = LSB of VX before shift
					V[0xF] = V[(opcode & 0x0F00) >> 8] & 0x1;
					V[(opcode & 0x0F00) >> 8] >>= 1;
					break;
				case 0x0007: // 8XY7 - Set VX to VY-VX. VF = 0 if there is a borrow
					V[0xF] = (V[(opcode & 0x0F00) >> 8] > V[(opcode & 0x00F0) >> 4]) ? 0 : 1;
					V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x00F0) >> 4] - V[(opcode & 0x0F00) >> 8];
					break;
				case 0x000E: // 8XYE - Shift VX left by 1. VF = MSB of VX before shift
					V[0xF] = V[(opcode & 0x0F00) >> 8] >> 7;
					V[(opcode & 0x0F00) >> 8] <<= 1;
					break;
				default:
					fprintf(stderr, "Unknown opcode: %x\n", opcode);
					return -1;
			}
			break;
		case 0x9000: // 9XY0 - Skip next instruction if VX != VY
			if (V[(opcode & 0x0F00) >> 8] != V[(opcode & 0x00F0) >> 4]) pc += 2;
			break;
		case 0xA000: // ANNN - Set I to the address NNN
			I = opcode & 0x0FFF;
			break;
		case 0xB000: // BNNN - Jump to NNN + V0
			pc = ((opcode & 0x0FFF) + V[0]) - 2;
			break;
		case 0xC000: // CNNN - Set VX to random number masked by NN
			V[(opcode & 0x0F00) >> 8] = (rand() % (0xFF + 1)) & (opcode & 0x00FF);
			break;
		case 0xD000: // Draw an 8 pixel sprite at (VX, VY)
		{
			uint8_t VX = V[(opcode & 0x0F00) >> 8];
			uint8_t VY = V[(opcode & 0x00F0) >> 4];
			uint16_t h = opcode & 0x000F;
			uint16_t pixel;

			V[0xF] = 0;
			int yl, xl;
			for (yl = 0; yl < h; yl++)
			{
				printf("Decoding: %x\n", opcode);
				pixel = memory[I + yl];
				for (xl = 0; xl < 8; xl++)
				{
					printf("Decoding: %x\n", opcode);
					printf("VX: %x\n", VX);
					printf("VY: %x\n", VY);
					printf("Pixel: %d\n", pixel);
					printf("yline: %d\txline: %d\n", yl, xl);

					if ((pixel & (0x80 >> xl)) != 0)
					{
						if (gfx[VX + xl + ((VY + yl) * 64)] == 1)
							V[0xF] = 1;
						gfx[VX + xl + ((VY + yl) * 64)] ^= 1;
						printf("gfx: %d\n", gfx[VX + xl + ((VY + yl) * 64)]);
					}
				}
			}

			drawflag = 1;
		}
			break;
		case 0xE000: // EX__
			switch (opcode & 0x00FF)
			{
				case 0x009E: // EX9E - Skip next instruction if key in VX is pressed
					if (keys[V[(opcode & 0x0F00) >> 8]] != 0) pc += 2;
					break;
				case 0x00A1: // EXA1 - Skip next instruction if key in VX isn't pressed
					if (keys[V[(opcode & 0x0F00) >> 8]] == 0) pc += 2;
					break;
				default:
					fprintf(stderr, "Unknown opcode: %x\n", opcode);
					return -1;
			}
			break;
		case 0xF000: // FX__
			switch (opcode & 0x00FF)
			{
				case 0x0007: // FX07 - Set VX to delaytimer
					V[(opcode & 0x0F00) >> 8] = delaytimer;
					break;
				case 0x000A: // FX0A - Wait for key press and then store it in VX
				{
					int keypressed = 0;
					for (i = 0; i < 16; i++)
					{
						if (keys[i] != 0)
						{
							V[(opcode & 0x0F00) >> 8] = i;
							keypressed = 1;
						}
					}

					if (!keypressed) return -1;
				}
					break;
				case 0x0015: // FX15 - Set the delaytimer to VX
					delaytimer = V[(opcode & 0x0F00) >> 8];
					break;
				case 0x0018: // FX18 - Set the soundtimer to VX
					soundtimer = V[(opcode & 0x0F00) >> 8];
					break;
				case 0x001E: // FX1E - Add VX to I
					V[0xF] = ((I + V[(opcode & 0x0F00) >> 8]) > 0xFFF) ? 1 : 0;
					I += V[(opcode & 0x0F00) >> 8];
					break;
				case 0x0029: // FX29 - Set I to the location of the sprite for char VX
					I = V[(opcode & 0x0F00) >> 8] * 0x5;
					break;
				case 0x0033: // FX33 - Store bin coded decimal of VX at I, I+1 and I+2
					memory[I]	= V[(opcode & 0x0F00) >> 8] / 100;
					memory[I+1]	= (V[(opcode & 0x0F00) >> 8] / 10) % 10;
					memory[I+2]	= V[(opcode & 0x0F00) >> 8] % 10;
					break;
				case 0x0055: // FX55 - Store V0 to VX in memory starting at I
					for (i = 0; i <= ((opcode & 0x0F00) >> 8); i++)
						memory[I + i] = V[i];
					I += ((opcode & 0x0F00) >> 8) + 1;
					break;
				case 0x0065: // FX65 - Fill V0 to VX with vals from memory starting at I
					for (i = 0; i <= ((opcode & 0x0F00) >> 8); i++)
						V[i] = memory[I + i];
					I += ((opcode & 0x0F00) >> 8) + 1;
					break;
				default:
					fprintf(stderr, "Unknown opcode: %x\n", opcode);
					return -1;
			}
			break;
		default:
			fprintf(stderr, "Unimplemented opcode\n");
			return -1;
	}

	return 1;
}

void
execute(Chip8 *chip8)
{
	pc += 2;
}

void
update_timers(Chip8 *chip8)
{
	if (delaytimer > 0) --delaytimer;
	if (soundtimer > 0) --soundtimer;
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
