#include <stdio.h>

#include "cpu_6502.h"
#include "log.h"
//#include "timer.h"
#include <string>

//
// The core of this code comes from http://rubbermallet.org/fake6502.c (c) 2011 Mike Chambers.
//
// Note this uses MAME style memory handling. The address passed to the user routines starts from 0.

#define bget(p,m) ((p) & (m))

// Timing Table from FakeNes
static const uint32_t ticks[256] = {
	/*        |  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  A  |  B  |  C  |  D  |  E  |  F  |     */
	/* 0 */      7,    6,    2,    8,    3,    3,    5,    5,    3,    2,    2,    2,    4,    4,    6,    6,  /* 0 */
	/* 1 */      2,    5,    2,    8,    4,    4,    6,    6,    2,    4,    2,    7,    4,    4,    7,    7,  /* 1 */
	/* 2 */      6,    6,    2,    8,    3,    3,    5,    5,    4,    2,    2,    2,    4,    4,    6,    6,  /* 2 */
	/* 3 */      2,    5,    2,    8,    4,    4,    6,    6,    2,    4,    2,    7,    4,    4,    7,    7,  /* 3 */
	/* 4 */      6,    6,    2,    8,    3,    3,    5,    5,    3,    2,    2,    2,    3,    4,    6,    6,  /* 4 */
	/* 5 */      2,    5,    2,    8,    4,    4,    6,    6,    2,    4,    2,    7,    4,    4,    7,    7,  /* 5 */
	/* 6 */      6,    6,    2,    8,    3,    3,    5,    5,    4,    2,    2,    2,    5,    4,    6,    6,  /* 6 */
	/* 7 */      2,    5,    2,    8,    4,    4,    6,    6,    2,    4,    2,    7,    4,    4,    7,    7,  /* 7 */
	/* 8 */      2,    6,    2,    6,    3,    3,    3,    3,    2,    2,    2,    2,    4,    4,    4,    4,  /* 8 */
	/* 9 */      2,    6,    2,    6,    4,    4,    4,    4,    2,    5,    2,    5,    5,    5,    5,    5,  /* 9 */
	/* A */      2,    6,    2,    6,    3,    3,    3,    3,    2,    2,    2,    2,    4,    4,    4,    4,  /* A */
	/* B */      2,    5,    2,    5,    4,    4,    4,    4,    2,    4,    2,    4,    4,    4,    4,    4,  /* B */
	/* C */      2,    6,    2,    8,    3,    3,    5,    5,    2,    2,    2,    2,    4,    4,    6,    6,  /* C */
	/* D */      2,    5,    2,    8,    4,    4,    6,    6,    2,    4,    2,    7,    4,    4,    7,    7,  /* D */
	/* E */      2,    6,    2,    8,    3,    3,    5,    5,    2,    2,    2,    2,    4,    4,    6,    6,  /* E */
	/* F */      2,    5,    2,    8,    4,    4,    6,    6,    2,    4,    2,    7,    4,    4,    7,    7   /* F */
};

cpu_6502::cpu_6502(uint8_t* mem, MemoryReadByte* read_mem, MemoryWriteByte* write_mem, uint16_t addr, int num)
{
	MEM = mem;
	memory_write = write_mem;
	memory_read = read_mem;
	init6502(addr);
	cpu_num = num;
}

int cpu_6502::get6502ticks(int reset)
{
	int tmp;

	tmp = clocktickstotal;
	if (reset)
	{
		clocktickstotal = 0;
	}
	return tmp;
}

uint8_t cpu_6502::read6502(uint16_t addr)
{
	uint8_t temp = 0;
	// Pointer to Beginning of our handler
	MemoryReadByte* MemRead = memory_read;

	while (MemRead->lowAddr != 0xffffffff)
	{
		if ((addr >= MemRead->lowAddr) && (addr <= MemRead->highAddr))
		{
			if (MemRead->memoryCall)
			{
				temp = MemRead->memoryCall(addr - MemRead->lowAddr, MemRead);
			}
			else
			{
				temp = *((uint8_t*)MemRead->pUserArea + (addr - MemRead->lowAddr));
			}
			MemRead = nullptr;
			break;
		}
		++MemRead;
	}
	// Add blocking here
	if (MemRead && !mmem)
	{
		temp = MEM[addr];
	}
	if (MemRead && mmem)
	{
		if (log_debug_rw) wrlog("Warning! Unhandled Read at %x", addr);
	}

	return temp;
}

void cpu_6502::write6502(uint16_t addr, uint8_t byte)
{
	// Pointer to Beginning of our handler
	MemoryWriteByte* MemWrite = memory_write;

	while (MemWrite->lowAddr != 0xffffffff)
	{
		if ((addr >= MemWrite->lowAddr) && (addr <= MemWrite->highAddr))
		{
			if (MemWrite->memoryCall)
			{
				MemWrite->memoryCall(addr - MemWrite->lowAddr, byte, MemWrite);
			}
			else
			{
				*((uint8_t*)MemWrite->pUserArea + (addr - MemWrite->lowAddr)) = byte;
			}
			MemWrite = nullptr;
			break;
		}
		++MemWrite;
	}
	// Add blocking here
	if (MemWrite && !mmem)
	{
		MEM[addr] = (uint8_t)byte;
	}
	if (MemWrite && mmem)
	{
		if (log_debug_rw) wrlog("Warning! Unhandled Write at %x data: %x", addr, byte);
	}
}

//a few general functions used by various other functions
void cpu_6502::push16(uint16_t pushval) {
	write6502(BASE_STACK + sp, (pushval >> 8) & 0xFF);
	write6502(BASE_STACK + ((sp - 1) & 0xFF), pushval & 0xFF);
	sp -= 2;
}

void cpu_6502::push8(uint8_t pushval) {
	write6502(BASE_STACK + sp--, pushval);
}

uint16_t cpu_6502::pull16() {
	uint16_t temp16;
	temp16 = read6502(BASE_STACK + ((sp + 1) & 0xFF)) | ((uint16_t)read6502(BASE_STACK + ((sp + 2) & 0xFF)) << 8);
	sp += 2;
	return(temp16);
}

uint8_t cpu_6502::pull8() {
	return (read6502(BASE_STACK + ++sp));
}

// Init MyCpu
void cpu_6502::init6502(uint16_t addrmaskval)
{
	mmem = false;
	log_debug_rw = false;

	int Iperiod = 0;
	int otherTicks = 0;
	pc = 0;
	ppc = 0;
	addrmask = addrmaskval;
	a = 0;
	x = 0;
	y = 0;
	status = 0;
	sp = 0;
	num = 0;
	reladdr = 0;
	opcode = 0;
	clockticks6502 = 0;
	clocktickstotal = 0;

	instruction[0x00] = &cpu_6502::brk; adrmode[0x00] = &cpu_6502::imp;
	instruction[0x01] = &cpu_6502::ora; adrmode[0x01] = &cpu_6502::indx;
	instruction[0x02] = &cpu_6502::nop; adrmode[0x02] = &cpu_6502::imp;
	instruction[0x03] = &cpu_6502::nop; adrmode[0x03] = &cpu_6502::imp;
	instruction[0x04] = &cpu_6502::nop; adrmode[0x04] = &cpu_6502::zp;
	instruction[0x05] = &cpu_6502::ora; adrmode[0x05] = &cpu_6502::zp;
	instruction[0x06] = &cpu_6502::asl; adrmode[0x06] = &cpu_6502::zp;
	instruction[0x07] = &cpu_6502::nop; adrmode[0x07] = &cpu_6502::imp;
	instruction[0x08] = &cpu_6502::php; adrmode[0x08] = &cpu_6502::imp;
	instruction[0x09] = &cpu_6502::ora; adrmode[0x09] = &cpu_6502::imm;
	instruction[0x0a] = &cpu_6502::asl; adrmode[0x0a] = &cpu_6502::acc;
	instruction[0x0b] = &cpu_6502::nop; adrmode[0x0b] = &cpu_6502::imp;
	instruction[0x0c] = &cpu_6502::nop; adrmode[0x0c] = &cpu_6502::abso;
	instruction[0x0d] = &cpu_6502::ora; adrmode[0x0d] = &cpu_6502::abso;
	instruction[0x0e] = &cpu_6502::asl; adrmode[0x0e] = &cpu_6502::abso;
	instruction[0x0f] = &cpu_6502::slo; adrmode[0x0f] = &cpu_6502::abso;
	instruction[0x10] = &cpu_6502::bpl; adrmode[0x10] = &cpu_6502::rel;
	instruction[0x11] = &cpu_6502::ora; adrmode[0x11] = &cpu_6502::indy;
	instruction[0x12] = &cpu_6502::nop; adrmode[0x12] = &cpu_6502::imp;
	instruction[0x13] = &cpu_6502::nop; adrmode[0x13] = &cpu_6502::imp;
	instruction[0x14] = &cpu_6502::nop; adrmode[0x14] = &cpu_6502::zpx;
	instruction[0x15] = &cpu_6502::ora; adrmode[0x15] = &cpu_6502::zpx;
	instruction[0x16] = &cpu_6502::asl; adrmode[0x16] = &cpu_6502::zpx;
	instruction[0x17] = &cpu_6502::nop; adrmode[0x17] = &cpu_6502::zpx;
	instruction[0x18] = &cpu_6502::clc; adrmode[0x18] = &cpu_6502::imp;
	instruction[0x19] = &cpu_6502::ora; adrmode[0x19] = &cpu_6502::absy;
	instruction[0x1a] = &cpu_6502::nop; adrmode[0x1a] = &cpu_6502::imp;
	instruction[0x1b] = &cpu_6502::nop; adrmode[0x1b] = &cpu_6502::imp;
	instruction[0x1c] = &cpu_6502::nop; adrmode[0x1c] = &cpu_6502::absx;
	instruction[0x1d] = &cpu_6502::ora; adrmode[0x1d] = &cpu_6502::absx;
	instruction[0x1e] = &cpu_6502::asl; adrmode[0x1e] = &cpu_6502::absx;
	instruction[0x1f] = &cpu_6502::nop; adrmode[0x1f] = &cpu_6502::imp;
	instruction[0x20] = &cpu_6502::jsr; adrmode[0x20] = &cpu_6502::abso;
	instruction[0x21] = &cpu_6502::op_and; adrmode[0x21] = &cpu_6502::indx;
	instruction[0x22] = &cpu_6502::nop; adrmode[0x22] = &cpu_6502::imp;
	instruction[0x23] = &cpu_6502::nop; adrmode[0x23] = &cpu_6502::imp;
	instruction[0x24] = &cpu_6502::op_bit; adrmode[0x24] = &cpu_6502::zp;
	instruction[0x25] = &cpu_6502::op_and; adrmode[0x25] = &cpu_6502::zp;
	instruction[0x26] = &cpu_6502::rol; adrmode[0x26] = &cpu_6502::zp;
	instruction[0x27] = &cpu_6502::nop; adrmode[0x27] = &cpu_6502::imp;
	instruction[0x28] = &cpu_6502::plp; adrmode[0x28] = &cpu_6502::imp;
	instruction[0x29] = &cpu_6502::op_and; adrmode[0x29] = &cpu_6502::imm;
	instruction[0x2a] = &cpu_6502::rol; adrmode[0x2a] = &cpu_6502::acc;
	instruction[0x2b] = &cpu_6502::nop; adrmode[0x2b] = &cpu_6502::imp;
	instruction[0x2c] = &cpu_6502::op_bit; adrmode[0x2c] = &cpu_6502::abso;
	instruction[0x2d] = &cpu_6502::op_and; adrmode[0x2d] = &cpu_6502::abso;
	instruction[0x2e] = &cpu_6502::rol; adrmode[0x2e] = &cpu_6502::abso;
	instruction[0x2f] = &cpu_6502::nop; adrmode[0x2f] = &cpu_6502::imp;
	instruction[0x30] = &cpu_6502::bmi; adrmode[0x30] = &cpu_6502::rel;
	instruction[0x31] = &cpu_6502::op_and; adrmode[0x31] = &cpu_6502::indy;
	instruction[0x32] = &cpu_6502::nop; adrmode[0x32] = &cpu_6502::imp;
	instruction[0x33] = &cpu_6502::nop; adrmode[0x33] = &cpu_6502::imp;
	instruction[0x34] = &cpu_6502::nop; adrmode[0x34] = &cpu_6502::zpx;
	instruction[0x35] = &cpu_6502::op_and; adrmode[0x35] = &cpu_6502::zpx;
	instruction[0x36] = &cpu_6502::rol; adrmode[0x36] = &cpu_6502::zpx;
	instruction[0x37] = &cpu_6502::nop; adrmode[0x37] = &cpu_6502::imp;
	instruction[0x38] = &cpu_6502::sec; adrmode[0x38] = &cpu_6502::imp;
	instruction[0x39] = &cpu_6502::op_and; adrmode[0x39] = &cpu_6502::absy;
	instruction[0x3a] = &cpu_6502::nop; adrmode[0x3a] = &cpu_6502::imp;
	instruction[0x3b] = &cpu_6502::nop; adrmode[0x3b] = &cpu_6502::imp;
	instruction[0x3c] = &cpu_6502::nop; adrmode[0x3c] = &cpu_6502::absx;
	instruction[0x3d] = &cpu_6502::op_and; adrmode[0x3d] = &cpu_6502::absx;
	instruction[0x3e] = &cpu_6502::rol; adrmode[0x3e] = &cpu_6502::absx;
	instruction[0x3f] = &cpu_6502::nop; adrmode[0x3f] = &cpu_6502::imp;
	instruction[0x40] = &cpu_6502::rti; adrmode[0x40] = &cpu_6502::imp;
	instruction[0x41] = &cpu_6502::eor; adrmode[0x41] = &cpu_6502::indx;
	instruction[0x42] = &cpu_6502::nop; adrmode[0x42] = &cpu_6502::imp;
	instruction[0x43] = &cpu_6502::nop; adrmode[0x43] = &cpu_6502::imp;
	instruction[0x44] = &cpu_6502::nop; adrmode[0x44] = &cpu_6502::imp;
	instruction[0x45] = &cpu_6502::eor; adrmode[0x45] = &cpu_6502::zp;
	instruction[0x46] = &cpu_6502::lsr; adrmode[0x46] = &cpu_6502::zp;
	instruction[0x47] = &cpu_6502::nop; adrmode[0x47] = &cpu_6502::imp;
	instruction[0x48] = &cpu_6502::pha; adrmode[0x48] = &cpu_6502::imp;
	instruction[0x49] = &cpu_6502::eor; adrmode[0x49] = &cpu_6502::imm;
	instruction[0x4a] = &cpu_6502::lsr; adrmode[0x4a] = &cpu_6502::acc;
	instruction[0x4b] = &cpu_6502::nop; adrmode[0x4b] = &cpu_6502::imp;
	instruction[0x4c] = &cpu_6502::jmp; adrmode[0x4c] = &cpu_6502::abso;
	instruction[0x4d] = &cpu_6502::eor; adrmode[0x4d] = &cpu_6502::abso;
	instruction[0x4e] = &cpu_6502::lsr; adrmode[0x4e] = &cpu_6502::abso;
	instruction[0x4f] = &cpu_6502::nop; adrmode[0x4f] = &cpu_6502::imp;
	instruction[0x50] = &cpu_6502::bvc; adrmode[0x50] = &cpu_6502::rel;
	instruction[0x51] = &cpu_6502::eor; adrmode[0x51] = &cpu_6502::indy;
	instruction[0x52] = &cpu_6502::nop; adrmode[0x52] = &cpu_6502::imp;
	instruction[0x53] = &cpu_6502::nop; adrmode[0x53] = &cpu_6502::imp;
	instruction[0x54] = &cpu_6502::nop; adrmode[0x54] = &cpu_6502::imp;
	instruction[0x55] = &cpu_6502::eor; adrmode[0x55] = &cpu_6502::zpx;
	instruction[0x56] = &cpu_6502::lsr; adrmode[0x56] = &cpu_6502::zpx;
	instruction[0x57] = &cpu_6502::nop; adrmode[0x57] = &cpu_6502::imp;
	instruction[0x58] = &cpu_6502::cli; adrmode[0x58] = &cpu_6502::imp;
	instruction[0x59] = &cpu_6502::eor; adrmode[0x59] = &cpu_6502::absy;
	instruction[0x5a] = &cpu_6502::nop; adrmode[0x5a] = &cpu_6502::imp;
	instruction[0x5b] = &cpu_6502::nop; adrmode[0x5b] = &cpu_6502::imp;
	instruction[0x5c] = &cpu_6502::nop; adrmode[0x5c] = &cpu_6502::imp;
	instruction[0x5d] = &cpu_6502::eor; adrmode[0x5d] = &cpu_6502::absx;
	instruction[0x5e] = &cpu_6502::lsr; adrmode[0x5e] = &cpu_6502::absx;
	instruction[0x5f] = &cpu_6502::nop; adrmode[0x5f] = &cpu_6502::imp;
	instruction[0x60] = &cpu_6502::rts; adrmode[0x60] = &cpu_6502::imp;
	instruction[0x61] = &cpu_6502::adc; adrmode[0x61] = &cpu_6502::indx;
	instruction[0x62] = &cpu_6502::nop; adrmode[0x62] = &cpu_6502::imp;
	instruction[0x63] = &cpu_6502::nop; adrmode[0x63] = &cpu_6502::imp;
	instruction[0x64] = &cpu_6502::nop; adrmode[0x64] = &cpu_6502::zp;
	instruction[0x65] = &cpu_6502::adc; adrmode[0x65] = &cpu_6502::zp;
	instruction[0x66] = &cpu_6502::ror; adrmode[0x66] = &cpu_6502::zp;
	instruction[0x67] = &cpu_6502::nop; adrmode[0x67] = &cpu_6502::imp;
	instruction[0x68] = &cpu_6502::pla; adrmode[0x68] = &cpu_6502::imp;
	instruction[0x69] = &cpu_6502::adc; adrmode[0x69] = &cpu_6502::imm;
	instruction[0x6a] = &cpu_6502::ror; adrmode[0x6a] = &cpu_6502::acc;
	instruction[0x6b] = &cpu_6502::nop; adrmode[0x6b] = &cpu_6502::imp;
	instruction[0x6c] = &cpu_6502::jmp; adrmode[0x6c] = &cpu_6502::ind;
	instruction[0x6d] = &cpu_6502::adc; adrmode[0x6d] = &cpu_6502::abso;
	instruction[0x6e] = &cpu_6502::ror; adrmode[0x6e] = &cpu_6502::abso;
	instruction[0x6f] = &cpu_6502::nop; adrmode[0x6f] = &cpu_6502::imp;
	instruction[0x70] = &cpu_6502::bvs; adrmode[0x70] = &cpu_6502::rel;
	instruction[0x71] = &cpu_6502::adc; adrmode[0x71] = &cpu_6502::indy;
	instruction[0x72] = &cpu_6502::nop; adrmode[0x72] = &cpu_6502::imp;
	instruction[0x73] = &cpu_6502::nop; adrmode[0x73] = &cpu_6502::imp;
	instruction[0x74] = &cpu_6502::nop; adrmode[0x74] = &cpu_6502::zpx;
	instruction[0x75] = &cpu_6502::adc; adrmode[0x75] = &cpu_6502::zpx;
	instruction[0x76] = &cpu_6502::ror; adrmode[0x76] = &cpu_6502::zpx;
	instruction[0x77] = &cpu_6502::nop; adrmode[0x77] = &cpu_6502::imp;
	instruction[0x78] = &cpu_6502::sei; adrmode[0x78] = &cpu_6502::imp;
	instruction[0x79] = &cpu_6502::adc; adrmode[0x79] = &cpu_6502::absy;
	instruction[0x7a] = &cpu_6502::nop; adrmode[0x7a] = &cpu_6502::imp;
	instruction[0x7b] = &cpu_6502::nop; adrmode[0x7b] = &cpu_6502::imp;
	instruction[0x7c] = &cpu_6502::nop; adrmode[0x7c] = &cpu_6502::absx;
	instruction[0x7d] = &cpu_6502::adc; adrmode[0x7d] = &cpu_6502::absx;
	instruction[0x7e] = &cpu_6502::ror; adrmode[0x7e] = &cpu_6502::absx;
	instruction[0x7f] = &cpu_6502::nop; adrmode[0x7f] = &cpu_6502::imp;
	instruction[0x80] = &cpu_6502::nop; adrmode[0x80] = &cpu_6502::imm;
	instruction[0x81] = &cpu_6502::sta; adrmode[0x81] = &cpu_6502::indx;
	instruction[0x82] = &cpu_6502::nop; adrmode[0x82] = &cpu_6502::imp;
	instruction[0x83] = &cpu_6502::nop; adrmode[0x83] = &cpu_6502::imp;
	instruction[0x84] = &cpu_6502::sty; adrmode[0x84] = &cpu_6502::zp;
	instruction[0x85] = &cpu_6502::sta; adrmode[0x85] = &cpu_6502::zp;
	instruction[0x86] = &cpu_6502::stx; adrmode[0x86] = &cpu_6502::zp;
	instruction[0x87] = &cpu_6502::nop; adrmode[0x87] = &cpu_6502::imp;
	instruction[0x88] = &cpu_6502::dey; adrmode[0x88] = &cpu_6502::imp;
	instruction[0x89] = &cpu_6502::nop; adrmode[0x89] = &cpu_6502::imm;
	instruction[0x8a] = &cpu_6502::txa; adrmode[0x8a] = &cpu_6502::imp;
	instruction[0x8b] = &cpu_6502::nop; adrmode[0x8b] = &cpu_6502::imp;
	instruction[0x8c] = &cpu_6502::sty; adrmode[0x8c] = &cpu_6502::abso;
	instruction[0x8d] = &cpu_6502::sta; adrmode[0x8d] = &cpu_6502::abso;
	instruction[0x8e] = &cpu_6502::stx; adrmode[0x8e] = &cpu_6502::abso;
	instruction[0x8f] = &cpu_6502::nop; adrmode[0x8f] = &cpu_6502::imp;
	instruction[0x90] = &cpu_6502::bcc; adrmode[0x90] = &cpu_6502::rel;
	instruction[0x91] = &cpu_6502::sta; adrmode[0x91] = &cpu_6502::indy;
	instruction[0x92] = &cpu_6502::nop; adrmode[0x92] = &cpu_6502::imp;
	instruction[0x93] = &cpu_6502::nop; adrmode[0x93] = &cpu_6502::imp;
	instruction[0x94] = &cpu_6502::sty; adrmode[0x94] = &cpu_6502::zpx;
	instruction[0x95] = &cpu_6502::sta; adrmode[0x95] = &cpu_6502::zpx;
	instruction[0x96] = &cpu_6502::stx; adrmode[0x96] = &cpu_6502::zpy;
	instruction[0x97] = &cpu_6502::nop; adrmode[0x97] = &cpu_6502::imp;
	instruction[0x98] = &cpu_6502::tya; adrmode[0x98] = &cpu_6502::imp;
	instruction[0x99] = &cpu_6502::sta; adrmode[0x99] = &cpu_6502::absy;
	instruction[0x9a] = &cpu_6502::txs; adrmode[0x9a] = &cpu_6502::imp;
	instruction[0x9b] = &cpu_6502::nop; adrmode[0x9b] = &cpu_6502::imp;
	instruction[0x9c] = &cpu_6502::nop; adrmode[0x9c] = &cpu_6502::absx;
	instruction[0x9d] = &cpu_6502::sta; adrmode[0x9d] = &cpu_6502::absx;
	instruction[0x9e] = &cpu_6502::nop; adrmode[0x9e] = &cpu_6502::absy;
	instruction[0x9f] = &cpu_6502::nop; adrmode[0x9f] = &cpu_6502::imp;
	instruction[0xa0] = &cpu_6502::ldy; adrmode[0xa0] = &cpu_6502::imm;
	instruction[0xa1] = &cpu_6502::lda; adrmode[0xa1] = &cpu_6502::indx;
	instruction[0xa2] = &cpu_6502::ldx; adrmode[0xa2] = &cpu_6502::imm;
	instruction[0xa3] = &cpu_6502::nop; adrmode[0xa3] = &cpu_6502::imp;
	instruction[0xa4] = &cpu_6502::ldy; adrmode[0xa4] = &cpu_6502::zp;
	instruction[0xa5] = &cpu_6502::lda; adrmode[0xa5] = &cpu_6502::zp;
	instruction[0xa6] = &cpu_6502::ldx; adrmode[0xa6] = &cpu_6502::zp;
	instruction[0xa7] = &cpu_6502::nop; adrmode[0xa7] = &cpu_6502::imp;
	instruction[0xa8] = &cpu_6502::tay; adrmode[0xa8] = &cpu_6502::imp;
	instruction[0xa9] = &cpu_6502::lda; adrmode[0xa9] = &cpu_6502::imm;
	instruction[0xaa] = &cpu_6502::tax; adrmode[0xaa] = &cpu_6502::imp;
	instruction[0xab] = &cpu_6502::nop; adrmode[0xab] = &cpu_6502::imp;
	instruction[0xac] = &cpu_6502::ldy; adrmode[0xac] = &cpu_6502::abso;
	instruction[0xad] = &cpu_6502::lda; adrmode[0xad] = &cpu_6502::abso;
	instruction[0xae] = &cpu_6502::ldx; adrmode[0xae] = &cpu_6502::abso;
	instruction[0xaf] = &cpu_6502::nop; adrmode[0xaf] = &cpu_6502::imp;
	instruction[0xb0] = &cpu_6502::bcs; adrmode[0xb0] = &cpu_6502::rel;
	instruction[0xb1] = &cpu_6502::lda; adrmode[0xb1] = &cpu_6502::indy;
	instruction[0xb2] = &cpu_6502::nop; adrmode[0xb2] = &cpu_6502::imp;
	instruction[0xb3] = &cpu_6502::nop; adrmode[0xb3] = &cpu_6502::imp;
	instruction[0xb4] = &cpu_6502::ldy; adrmode[0xb4] = &cpu_6502::zpx;
	instruction[0xb5] = &cpu_6502::lda; adrmode[0xb5] = &cpu_6502::zpx;
	instruction[0xb6] = &cpu_6502::ldx; adrmode[0xb6] = &cpu_6502::zpy;
	instruction[0xb7] = &cpu_6502::nop; adrmode[0xb7] = &cpu_6502::imp;
	instruction[0xb8] = &cpu_6502::clv; adrmode[0xb8] = &cpu_6502::imp;
	instruction[0xb9] = &cpu_6502::lda; adrmode[0xb9] = &cpu_6502::absy;
	instruction[0xba] = &cpu_6502::tsx; adrmode[0xba] = &cpu_6502::imp;
	instruction[0xbb] = &cpu_6502::nop; adrmode[0xbb] = &cpu_6502::imp;
	instruction[0xbc] = &cpu_6502::ldy; adrmode[0xbc] = &cpu_6502::absx;
	instruction[0xbd] = &cpu_6502::lda; adrmode[0xbd] = &cpu_6502::absx;
	instruction[0xbe] = &cpu_6502::ldx; adrmode[0xbe] = &cpu_6502::absy;
	instruction[0xbf] = &cpu_6502::nop; adrmode[0xbf] = &cpu_6502::imp;
	instruction[0xc0] = &cpu_6502::cpy; adrmode[0xc0] = &cpu_6502::imm;
	instruction[0xc1] = &cpu_6502::cmp; adrmode[0xc1] = &cpu_6502::indx;
	instruction[0xc2] = &cpu_6502::nop; adrmode[0xc2] = &cpu_6502::imp;
	instruction[0xc3] = &cpu_6502::nop; adrmode[0xc3] = &cpu_6502::imp;
	instruction[0xc4] = &cpu_6502::cpy; adrmode[0xc4] = &cpu_6502::zp;
	instruction[0xc5] = &cpu_6502::cmp; adrmode[0xc5] = &cpu_6502::zp;
	instruction[0xc6] = &cpu_6502::dec; adrmode[0xc6] = &cpu_6502::zp;
	instruction[0xc7] = &cpu_6502::dcp; adrmode[0xc7] = &cpu_6502::zp;
	instruction[0xc8] = &cpu_6502::iny; adrmode[0xc8] = &cpu_6502::imp;
	instruction[0xc9] = &cpu_6502::cmp; adrmode[0xc9] = &cpu_6502::imm;
	instruction[0xca] = &cpu_6502::dex; adrmode[0xca] = &cpu_6502::imp;
	instruction[0xcb] = &cpu_6502::nop; adrmode[0xcb] = &cpu_6502::imp;
	instruction[0xcc] = &cpu_6502::cpy; adrmode[0xcc] = &cpu_6502::abso;
	instruction[0xcd] = &cpu_6502::cmp; adrmode[0xcd] = &cpu_6502::abso;
	instruction[0xce] = &cpu_6502::dec; adrmode[0xce] = &cpu_6502::abso;
	instruction[0xcf] = &cpu_6502::nop; adrmode[0xcf] = &cpu_6502::imp;
	instruction[0xd0] = &cpu_6502::bne; adrmode[0xd0] = &cpu_6502::rel;
	instruction[0xd1] = &cpu_6502::cmp; adrmode[0xd1] = &cpu_6502::indy;
	instruction[0xd2] = &cpu_6502::nop; adrmode[0xd2] = &cpu_6502::imp;
	instruction[0xd3] = &cpu_6502::dcp; adrmode[0xd3] = &cpu_6502::indy;
	instruction[0xd4] = &cpu_6502::nop; adrmode[0xd4] = &cpu_6502::imp;
	instruction[0xd5] = &cpu_6502::cmp; adrmode[0xd5] = &cpu_6502::zpx;
	instruction[0xd6] = &cpu_6502::dec; adrmode[0xd6] = &cpu_6502::zpx;
	instruction[0xd7] = &cpu_6502::nop; adrmode[0xd7] = &cpu_6502::imp;
	instruction[0xd8] = &cpu_6502::cld; adrmode[0xd8] = &cpu_6502::imp;
	instruction[0xd9] = &cpu_6502::cmp; adrmode[0xd9] = &cpu_6502::absy;
	instruction[0xda] = &cpu_6502::nop; adrmode[0xda] = &cpu_6502::imp;
	instruction[0xdb] = &cpu_6502::nop; adrmode[0xdb] = &cpu_6502::imp;
	instruction[0xdc] = &cpu_6502::nop; adrmode[0xdc] = &cpu_6502::imp;
	instruction[0xdd] = &cpu_6502::cmp; adrmode[0xdd] = &cpu_6502::absx;
	instruction[0xde] = &cpu_6502::dec; adrmode[0xde] = &cpu_6502::absx;
	instruction[0xdf] = &cpu_6502::nop; adrmode[0xdf] = &cpu_6502::imp;
	instruction[0xe0] = &cpu_6502::cpx; adrmode[0xe0] = &cpu_6502::imm;
	instruction[0xe1] = &cpu_6502::sbc; adrmode[0xe1] = &cpu_6502::indx;
	instruction[0xe2] = &cpu_6502::nop; adrmode[0xe2] = &cpu_6502::imp;
	instruction[0xe3] = &cpu_6502::nop; adrmode[0xe3] = &cpu_6502::imp;
	instruction[0xe4] = &cpu_6502::cpx; adrmode[0xe4] = &cpu_6502::zp;
	instruction[0xe5] = &cpu_6502::sbc; adrmode[0xe5] = &cpu_6502::zp;
	instruction[0xe6] = &cpu_6502::inc; adrmode[0xe6] = &cpu_6502::zp;
	instruction[0xe7] = &cpu_6502::nop; adrmode[0xe7] = &cpu_6502::imp;
	instruction[0xe8] = &cpu_6502::inx; adrmode[0xe8] = &cpu_6502::imp;
	instruction[0xe9] = &cpu_6502::sbc; adrmode[0xe9] = &cpu_6502::imm;
	instruction[0xea] = &cpu_6502::nop; adrmode[0xea] = &cpu_6502::imp;
	instruction[0xeb] = &cpu_6502::sbc; adrmode[0xeb] = &cpu_6502::imm; ////////THIS ONE
	instruction[0xec] = &cpu_6502::cpx; adrmode[0xec] = &cpu_6502::abso;
	instruction[0xed] = &cpu_6502::sbc; adrmode[0xed] = &cpu_6502::abso;
	instruction[0xee] = &cpu_6502::inc; adrmode[0xee] = &cpu_6502::abso;
	instruction[0xef] = &cpu_6502::nop; adrmode[0xef] = &cpu_6502::imp;
	instruction[0xf0] = &cpu_6502::beq; adrmode[0xf0] = &cpu_6502::rel;
	instruction[0xf1] = &cpu_6502::sbc; adrmode[0xf1] = &cpu_6502::indy;
	instruction[0xf2] = &cpu_6502::nop; adrmode[0xf2] = &cpu_6502::imp;
	instruction[0xf3] = &cpu_6502::nop; adrmode[0xf3] = &cpu_6502::imp;
	instruction[0xf4] = &cpu_6502::nop; adrmode[0xf4] = &cpu_6502::imp;
	instruction[0xf5] = &cpu_6502::sbc; adrmode[0xf5] = &cpu_6502::zpx;
	instruction[0xf6] = &cpu_6502::inc; adrmode[0xf6] = &cpu_6502::zpx;
	instruction[0xf7] = &cpu_6502::nop; adrmode[0xf7] = &cpu_6502::imp;
	instruction[0xf8] = &cpu_6502::sed; adrmode[0xf8] = &cpu_6502::imp;
	instruction[0xf9] = &cpu_6502::sbc; adrmode[0xf9] = &cpu_6502::absy;
	instruction[0xfa] = &cpu_6502::nop; adrmode[0xfa] = &cpu_6502::imp;
	instruction[0xfb] = &cpu_6502::nop; adrmode[0xfb] = &cpu_6502::imp;
	instruction[0xfc] = &cpu_6502::nop; adrmode[0xfc] = &cpu_6502::imp;
	instruction[0xfd] = &cpu_6502::sbc; adrmode[0xfd] = &cpu_6502::absx;
	instruction[0xfe] = &cpu_6502::inc; adrmode[0xfe] = &cpu_6502::absx;
	instruction[0xff] = &cpu_6502::nop; adrmode[0xff] = &cpu_6502::imp;
}

//Addressing Modes:

//addressing mode functions, calculates effective addresses
void cpu_6502::imp() { //implied
}

void cpu_6502::acc() { //accumulator
	useaccum = 1;
}

void cpu_6502::imm() { //immediate
	ea = pc++;
}

// I don't think the Zere Page accesses need to go through handlers. 
void cpu_6502::zp() { //zero-page
	ea = (uint16_t)read6502((uint16_t)pc++);
}

void cpu_6502::zpx() { //zero-page,X
	ea = ((uint16_t)read6502((uint16_t)pc++) + (uint16_t)x) & 0xFF; //zero-page wraparound
}

void cpu_6502::zpy() { //zero-page,Y
	ea = ((uint16_t)read6502((uint16_t)pc++) + (uint16_t)y) & 0xFF; //zero-page wraparound
}

void cpu_6502::rel() { //relative for branch ops (8-bit immediate value, sign-extended)
	reladdr = (uint16_t)read6502(pc++);
	if (reladdr & 0x80) reladdr |= 0xFF00;
}

void cpu_6502::abso() { //absolute
	ea = (uint16_t)read6502(pc) | ((uint16_t)read6502(pc + 1) << 8);
	pc += 2;
}

void cpu_6502::absx() { //absolute,X
	uint16_t startpage;
	ea = ((uint16_t)read6502(pc) | ((uint16_t)read6502(pc + 1) << 8));
	startpage = ea & 0xFF00;
	ea += (uint16_t)x;

	if (startpage != (ea & 0xFF00)) { //one cycle penlty for page-crossing on some opcodes
		penaltyaddr = 1;
	}

	pc += 2;
}

void cpu_6502::absy() { //absolute,Y
	uint16_t startpage;
	ea = ((uint16_t)read6502(pc) | ((uint16_t)read6502(pc + 1) << 8));
	startpage = ea & 0xFF00;
	ea += (uint16_t)y;

	if (startpage != (ea & 0xFF00)) { //one cycle penlty for page-crossing on some opcodes
		penaltyaddr = 1;
	}

	pc += 2;
}

void cpu_6502::ind() { //indirect
	uint16_t eahelp, eahelp2;
	eahelp = (uint16_t)read6502(pc) | (uint16_t)((uint16_t)read6502(pc + 1) << 8);
	eahelp2 = (eahelp & 0xFF00) | ((eahelp + 1) & 0x00FF); //replicate 6502 page-boundary wraparound bug
	ea = (uint16_t)read6502(eahelp) | ((uint16_t)read6502(eahelp2) << 8);
	pc += 2;
}

void cpu_6502::indx() { // (indirect,X)
	uint16_t eahelp;
	eahelp = (uint16_t)(((uint16_t)read6502(pc++) + (uint16_t)x) & 0xFF); //zero-page wraparound for table pointer
	ea = (uint16_t)read6502(eahelp & 0x00FF) | ((uint16_t)read6502((eahelp + 1) & 0x00FF) << 8);
}

void cpu_6502::indy() { // (indirect),Y
	uint16_t eahelp, eahelp2, startpage;
	eahelp = (uint16_t)read6502(pc++);
	eahelp2 = (eahelp & 0xFF00) | ((eahelp + 1) & 0x00FF); //zero-page wraparound
	ea = (uint16_t)read6502(eahelp) | ((uint16_t)read6502(eahelp2) << 8);
	startpage = ea & 0xFF00;
	ea += (uint16_t)y;

	if (startpage != (ea & 0xFF00)) { //one cycle penlty for page-crossing on some opcodes
		penaltyaddr = 1;
	}
}

uint16_t cpu_6502::getvalue() {
	if (useaccum) return((uint16_t)a);
	else return((uint16_t)read6502(ea));
}

uint16_t cpu_6502::getvalue16() {
	return((uint16_t)read6502(ea) | ((uint16_t)read6502(ea + 1) << 8));
}

void cpu_6502::putvalue(uint16_t saveval) {
	if (useaccum) a = (uint8_t)(saveval & 0x00FF);
	else write6502(ea, (saveval & 0x00FF));
}

//Instructions below here.
void cpu_6502::adc() //M.A.M.E. (tm) Code for the ADC.
{
	penaltyop = 1;
	int tmp = getvalue();

	if (status & FLAG_DECIMAL)
	{
		int c = (status & FLAG_CARRY);
		int lo = (a & 0x0f) + (tmp & 0x0f) + c;
		int hi = (a & 0xf0) + (tmp & 0xf0);
		status &= ~(FLAG_OVERFLOW | FLAG_CARRY);
		if (lo > 0x09)
		{
			hi += 0x10;
			lo += 0x06;
		}
		if (~(a ^ tmp) & (a ^ hi) & FLAG_SIGN)
			status |= FLAG_OVERFLOW;
		if (hi > 0x90)
			hi += 0x60;
		if (hi & 0xff00)
			status |= FLAG_CARRY;
		a = (lo & 0x0f) + (hi & 0xf0);
	}
	else
	{
		int c = (status & FLAG_CARRY);
		int sum = a + tmp + c;
		status &= ~(FLAG_OVERFLOW | FLAG_CARRY);
		if (~(a ^ tmp) & (a ^ sum) & FLAG_SIGN)
			status |= FLAG_OVERFLOW;
		if (sum & 0xff00)
			status |= FLAG_CARRY;
		a = (uint8_t)(sum & 0x00FF);

	}
	if (a) status &= ~FLAG_ZERO; else status |= FLAG_ZERO;
	if (a & FLAG_SIGN) status |= FLAG_SIGN; else status &= ~FLAG_SIGN;
}

void cpu_6502::op_and() {
	penaltyop = 1;
	value = getvalue();
	result = (uint16_t)a & value;

	zerocalc(result);
	signcalc(result);

	saveaccum(result);
}

void cpu_6502::asl() {
	value = getvalue();
	result = value << 1;

	carrycalc(result);
	zerocalc(result);
	signcalc(result);

	putvalue(result);
}

void cpu_6502::bcc() {
	if ((status & FLAG_CARRY) == 0) {
		oldpc = pc;
		pc += reladdr;
		if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
		else clockticks6502++;
	}
}

void cpu_6502::bcs() {
	if ((status & FLAG_CARRY) == FLAG_CARRY) {
		oldpc = pc;
		pc += reladdr;
		if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
		else clockticks6502++;
	}
}

void cpu_6502::beq() {
	if ((status & FLAG_ZERO) == FLAG_ZERO) {
		oldpc = pc;
		pc += reladdr;
		if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
		else clockticks6502++;
	}
}

void cpu_6502::op_bit() {
	value = getvalue();
	result = (uint16_t)a & value;

	zerocalc(result);
	status = (status & 0x3F) | (uint8_t)(value & 0xC0);
}

void cpu_6502::bmi() {
	if ((status & FLAG_SIGN) == FLAG_SIGN) {
		oldpc = pc;
		pc += reladdr;
		if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
		else clockticks6502++;
	}
}

void cpu_6502::bne() {
	if ((status & FLAG_ZERO) == 0) {
		oldpc = pc;
		pc += reladdr;
		if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
		else clockticks6502++;
	}
}

void cpu_6502::bpl() {
	if ((status & FLAG_SIGN) == 0) {
		oldpc = pc;
		pc += reladdr;
		if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
		else clockticks6502++;
	}
}

void cpu_6502::brk() {
	pc++;
	push16(pc); //push next instruction address onto stack
	push8(status | FLAG_BREAK); //push CPU status to stack
	// Klaus Dormann's test suite is expecting the flag register's bit 5 to be set
	status |= FLAG_CONSTANT;
	setinterrupt(); //set interrupt flag
	pc = (uint16_t)read6502(0xFFFE & addrmask) | ((uint16_t)read6502(0xFFFF & addrmask) << 8);
}

void cpu_6502::bvc() {
	if ((status & FLAG_OVERFLOW) == 0) {
		oldpc = pc;
		pc += reladdr;
		if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
		else clockticks6502++;
	}
}

void cpu_6502::bvs() {
	if ((status & FLAG_OVERFLOW) == FLAG_OVERFLOW) {
		oldpc = pc;
		pc += reladdr;
		if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
		else clockticks6502++;
	}
}

void cpu_6502::clc() {
	clearcarry();
}

void cpu_6502::cld() {
	cleardecimal();
}

void cpu_6502::cli() {
	clearinterrupt();
}

void cpu_6502::clv() {
	clearoverflow();
}

void cpu_6502::cmp() {
	penaltyop = 1;
	value = getvalue();
	result = (uint16_t)a - value;

	if (a >= (uint8_t)(value & 0x00FF)) setcarry();
	else clearcarry();
	if (a == (uint8_t)(value & 0x00FF)) setzero();
	else clearzero();
	signcalc(result);
}

void cpu_6502::cpx() {
	value = getvalue();
	result = (uint16_t)x - value;

	if (x >= (uint8_t)(value & 0x00FF)) setcarry();
	else clearcarry();
	if (x == (uint8_t)(value & 0x00FF)) setzero();
	else clearzero();
	signcalc(result);
}

void cpu_6502::cpy() {
	value = getvalue();
	result = (uint16_t)y - value;

	if (y >= (uint8_t)(value & 0x00FF)) setcarry();
	else clearcarry();
	if (y == (uint8_t)(value & 0x00FF)) setzero();
	else clearzero();
	signcalc(result);
}

void cpu_6502::dec() {
	value = getvalue();
	result = value - 1;

	zerocalc(result);
	signcalc(result);

	putvalue(result);
}

void cpu_6502::dex() {
	x--;

	zerocalc(x);
	signcalc(x);
}

void cpu_6502::dey() {
	y--;

	zerocalc(y);
	signcalc(y);
}

void cpu_6502::eor() {
	penaltyop = 1;
	value = getvalue();
	result = (uint16_t)a ^ value;

	zerocalc(result);
	signcalc(result);

	saveaccum(result);
}

void cpu_6502::inc() {
	value = getvalue();
	result = value + 1;

	zerocalc(result);
	signcalc(result);

	putvalue(result);
}

void cpu_6502::inx() {
	x++;

	zerocalc(x);
	signcalc(x);
}

void cpu_6502::iny() {
	y++;

	zerocalc(y);
	signcalc(y);
}

void cpu_6502::jmp() {
	pc = ea;
}

void cpu_6502::jsr() {
	push16(pc - 1);
	pc = ea;
}

void cpu_6502::lda() {
	penaltyop = 1;
	value = getvalue();
	a = (uint8_t)(value & 0x00FF);

	zerocalc(a);
	signcalc(a);
}

void cpu_6502::ldx() {
	penaltyop = 1;
	value = getvalue();
	x = (uint8_t)(value & 0x00FF);

	zerocalc(x);
	signcalc(x);
}

void cpu_6502::ldy() {
	penaltyop = 1;
	value = getvalue();
	y = (uint8_t)(value & 0x00FF);

	zerocalc(y);
	signcalc(y);
}

void cpu_6502::lsr() {
	value = getvalue();
	result = value >> 1;

	if (value & 1) setcarry();
	else clearcarry();
	zerocalc(result);
	signcalc(result);

	putvalue(result);
}

void cpu_6502::nop() {
	switch (opcode) {
	case 0x1C:
	case 0x3C:
	case 0x5C:
	case 0x7C:
	case 0xDC:
	case 0xFC:
		penaltyop = 1;
		break;
	}
}

void cpu_6502::ora() {
	penaltyop = 1;
	value = getvalue();
	result = (uint16_t)a | value;

	zerocalc(result);
	signcalc(result);

	saveaccum(result);
}

void cpu_6502::pha() {
	push8(a);
}

void cpu_6502::php() {
	push8(status | FLAG_BREAK);
}

void cpu_6502::pla() {
	a = pull8();

	zerocalc(a);
	signcalc(a);
}

void cpu_6502::plp() {
	status = pull8() | FLAG_CONSTANT;
}

void cpu_6502::rol() {
	value = getvalue();
	result = (value << 1) | (status & FLAG_CARRY);

	carrycalc(result);
	zerocalc(result);
	signcalc(result);

	putvalue(result);
}

void cpu_6502::ror() {
	value = getvalue();
	result = (value >> 1) | ((status & FLAG_CARRY) << 7);

	if (value & 1) setcarry();
	else clearcarry();
	zerocalc(result);
	signcalc(result);

	putvalue(result);
}

void cpu_6502::rti() {
	status = pull8();
	value = pull16();
	pc = value;
}

void cpu_6502::rts() {
	value = pull16();
	pc = value + 1;
}

//M.A.M.E. (tm) Code for the SBC.
void cpu_6502::sbc()
{
	penaltyop = 1;
	int tmp = getvalue();

	if (status & FLAG_DECIMAL)
	{
		int c = (status & FLAG_CARRY) ^ FLAG_CARRY;
		int sum = a - tmp - c;
		int lo = (a & 0x0f) - (tmp & 0x0f) - c;
		int hi = (a & 0xf0) - (tmp & 0xf0);
		if (lo & 0x10)
		{
			lo -= 6;
			hi--;
		}
		status &= ~(FLAG_OVERFLOW | FLAG_CARRY | FLAG_ZERO | FLAG_SIGN);
		if ((a ^ tmp) & (a ^ sum) & FLAG_SIGN)
			status |= FLAG_OVERFLOW;
		if (hi & 0x0100)
			hi -= 0x60;
		if ((sum & 0xff00) == 0)
			status |= FLAG_CARRY;
		if (!((a - tmp - c) & 0xff))
			status |= FLAG_ZERO;
		if ((a - tmp - c) & 0x80)
			status |= FLAG_SIGN;
		a = (lo & 0x0f) | (hi & 0xf0);
	}
	else
	{
		int c = (status & FLAG_CARRY) ^ FLAG_CARRY;
		int sum = a - tmp - c;
		status &= ~(FLAG_OVERFLOW | FLAG_CARRY);
		if ((a ^ tmp) & (a ^ sum) & FLAG_SIGN)
			status |= FLAG_OVERFLOW;
		if ((sum & 0xff00) == 0)
			status |= FLAG_CARRY;
		a = (uint8_t)(sum & 0x00FF);

		if (a) status &= ~FLAG_ZERO; else status |= FLAG_ZERO;
		if (a & FLAG_SIGN) status |= FLAG_SIGN; else status &= ~FLAG_SIGN;
	}
}

void cpu_6502::sec() {
	setcarry();
}

void cpu_6502::sed() {
	setdecimal();
}

void cpu_6502::sei() {
	setinterrupt();
}

void cpu_6502::sta() {
	putvalue(a);
}

void cpu_6502::stx() {
	putvalue(x);
}

void cpu_6502::sty() {
	putvalue(y);
}

void cpu_6502::tax() {
	x = a;

	zerocalc(x);
	signcalc(x);
}

void cpu_6502::tay() {
	y = a;

	zerocalc(y);
	signcalc(y);
}

void cpu_6502::tsx() {
	x = sp;

	zerocalc(x);
	signcalc(x);
}

void cpu_6502::txa() {
	a = x;

	zerocalc(a);
	signcalc(a);
}

void cpu_6502::txs() {
	sp = x;
}

void cpu_6502::tya() {
	a = y;

	zerocalc(a);
	signcalc(a);
}

//undocumented instructions

void cpu_6502::lax() {
	lda();
	ldx();
}

void cpu_6502::sax() {
	sta();
	stx();
	putvalue(a & x);
	if (penaltyop && penaltyaddr) clockticks6502--;
}

void cpu_6502::dcp() {
	dec();
	cmp();
	if (penaltyop && penaltyaddr) clockticks6502--;
}

void cpu_6502::isb() {
	inc();
	sbc();
	if (penaltyop && penaltyaddr) clockticks6502--;
}

void cpu_6502::slo() {
	asl();
	ora();
	if (penaltyop && penaltyaddr) clockticks6502--;
}

void cpu_6502::rla() {
	rol();
	op_and();
	if (penaltyop && penaltyaddr) clockticks6502--;
}

void cpu_6502::sre() {
	lsr();
	eor();
	if (penaltyop && penaltyaddr) clockticks6502--;
}

void cpu_6502::rra() {
	ror();
	adc();
	if (penaltyop && penaltyaddr) clockticks6502--;
}

uint8_t cpu_6502::get_reg(int regnum)
{
	switch (regnum)
	{
	case M6502_S: return sp;
	case M6502_P: return status;
	case M6502_A: return a;
	case M6502_X: return x;
	case M6502_Y: return y;
	}

	return 0;
}

void cpu_6502::m6502_set_reg(int regnum, uint8_t val)
{
	switch (regnum)
	{
	case M6502_S: sp = val; break;
	case M6502_P: status = val; break;
	case M6502_A: a = val; break;
	case M6502_X: x = val; break;
	case M6502_Y: y = val; break;
	default:break;
	}
}

uint16_t cpu_6502::get_pc()
{
	return pc;
}

uint16_t cpu_6502::get_ppc()
{
	return ppc;
}

void cpu_6502::set_pc(uint16_t _pc)
{
	pc = _pc;
}

// Reset MyCpu
void cpu_6502::reset6502()
{
	pc = (uint16_t)read6502(0xFFFC & addrmask) | ((uint16_t)read6502(0xFFFD & addrmask) << 8);
	a = 0;
	x = 0;
	y = 0;
	sp = 0xff;
	//status |= FLAG_CONSTANT;
	status = FLAG_CONSTANT | FLAG_INTERRUPT | FLAG_ZERO | FLAG_BREAK | (status & FLAG_DECIMAL); //Using MAME 6502 flag settings with decimal disable for now.
	clockticks6502 += 6; //RESET Takes 6 cycles
}

// Non maskerable interrupt
void cpu_6502::nmi6502()
{
	push16(pc);
	push8(status);
	status |= FLAG_INTERRUPT;
	pc = (uint16_t)read6502(0xFFFA & addrmask) | ((uint16_t)read6502(0xFFFB & addrmask) << 8);
	clockticks6502 += 7; //NMI Takes 7 cycles
	clocktickstotal += 7; //NMI Takes 7 cycles
}

// Maskerable Interrupt
void cpu_6502::irq6502()
{
	if (!(status & FLAG_INTERRUPT))	// pending_nmi != 0) || pending_irq != 0) Add later
	{
		push16(pc);
		push8(status & ~FLAG_BREAK);
		status |= FLAG_INTERRUPT;
		pc = (uint16_t)read6502(0xFFFE & addrmask) | ((uint16_t)read6502(0xFFFF & addrmask) << 8);
	}
	clockticks6502 += 7; //IRQ Takes 7 cycles
	clocktickstotal += 7; //IRQ Takes 7 cycles
}

int cpu_6502::exec6502(int timerTicks)
{
	clockticks6502 = 0;

	//wrlog("Timer Ticks here are %d", timerTicks);
	while (clockticks6502 < timerTicks)
	{
		int lastticks = clockticks6502;
		// fetch instruction
		opcode = read6502(pc++);
		status |= FLAG_CONSTANT;
		useaccum = 0;
		penaltyop = 0;
		penaltyaddr = 0;

		if (debug)
		{
			num++;
			std::string op = disam(opcode);
			int c = bget(status, FLAG_CARRY) ? 1 : 0;
			int z = bget(status, FLAG_ZERO) ? 1 : 0;
			int i = bget(status, FLAG_INTERRUPT) ? 1 : 0;
			int d = bget(status, FLAG_DECIMAL) ? 1 : 0;
			int b = bget(status, FLAG_BREAK) ? 1 : 0;
			int t = bget(status, FLAG_CONSTANT) ? 1 : 0;
			int v = bget(status, FLAG_OVERFLOW) ? 1 : 0;
			int n = bget(status, FLAG_SIGN) ? 1 : 0;
			wrlog("LN:%d  %x:  Opcode: %s data: %02x%02x   FLAGS: C:%d Z:%d I:%d D:%d B:%d T:%d V:%d N:%d", num, pc - 1, op.c_str(), MEM[pc + 1], MEM[pc], c, z, i, d, b, t, v, n);
		}

		if (opcode > 0xff)
		{
			wrlog("Invalid Opcode called!!!: opcode %x  Memory %X ", opcode, pc);
			return 0x80000000;
		}

		//Backup PC
		ppc = pc;
		//set addressing mode
		(this->*(adrmode[opcode]))();
		// execute instruction
		(this->*(instruction[opcode]))();
		// update clock cycles
		if (penaltyop && penaltyaddr) { clockticks6502++; clocktickstotal++; }
		clockticks6502 += ticks[opcode];
		clocktickstotal += ticks[opcode];
		//timer_update(clockticks6502 - lastticks, cpu_num);
		//wrlog("Cycles diff %d", clockticks6502-lastticks);

		if (clocktickstotal > 0xfffffff) clocktickstotal = 0;
	}
	return clockticks6502;
}

int cpu_6502::step6502()
{
	clockticks6502 = 0;
	status |= FLAG_CONSTANT;
	// fetch instruction
	opcode = opcode = read6502(pc++);
	if (opcode > 0xff)
	{
		wrlog("Invalid Opcode called!!!: opcode %x  Memory %X ", opcode, pc);
		return 0x80000000;
	}
	useaccum = 0;
	penaltyop = 0;
	penaltyaddr = 0;

	if (debug)
	{
		wrlog("opcode %x  data: %x%x Mem %x ", opcode, MEM[pc - 1], MEM[pc + 1], pc - 1);
		wrlog("REG A:%x X:%x Y:%x S:%x P:%x", a, x, y, sp, status);

		int c = bget(status, FLAG_CARRY) ? 1 : 0;
		int z = bget(status, FLAG_ZERO) ? 1 : 0;
		int i = bget(status, FLAG_INTERRUPT) ? 1 : 0;
		int d = bget(status, FLAG_DECIMAL) ? 1 : 0;
		int b = bget(status, FLAG_BREAK) ? 1 : 0;
		int t = bget(status, FLAG_CONSTANT) ? 1 : 0;
		int v = bget(status, FLAG_OVERFLOW) ? 1 : 0;
		int n = bget(status, FLAG_SIGN) ? 1 : 0;
		wrlog("FLAGS: C:%d Z:%d I:%d D:%d B:%d T:%d V:%d N:%d", c, z, i, d, b, t, v, n);
		wrlog("------------------------------------");
	}
	//Backup PC
	ppc = pc;
	//set addressing mode
	(this->*(adrmode[opcode]))();
	// execute instruction
	(this->*(instruction[opcode]))();
	// update clock cycles
	clockticks6502 += ticks[opcode];
	clocktickstotal += ticks[opcode];
	if (clocktickstotal > 0xfffffff) clocktickstotal = 0;

	return clockticks6502;
}

std::string cpu_6502::disam(uint8_t opcode)
{
	char opstr[64];

	std::string rval;

	switch (opcode)
	{
	case 0x00: sprintf_s(opstr, "BRK"); break;
	case 0x01: sprintf_s(opstr, "ORA ($%02x,X)", MEM[pc]);  break;
	case 0x04: sprintf_s(opstr, "TSB"); break;
	case 0x05: sprintf_s(opstr, "ORA $%02x", MEM[pc]);  break;
	case 0x06: sprintf_s(opstr, "ASL $%02x", MEM[pc]);  break;
	case 0x08: sprintf_s(opstr, "PHP"); break;
	case 0x09: sprintf_s(opstr, "ORA #$%02x", MEM[pc]);  break;
	case 0x0a: sprintf_s(opstr, "ASL A"); break;
	case 0x0d: sprintf_s(opstr, "ORA $%02x%02x", MEM[pc + 1], MEM[pc]); break;
	case 0x0e: sprintf_s(opstr, "ASL $%02x%02x", MEM[pc + 1], MEM[pc]); break;
	case 0x10: sprintf_s(opstr, "BPL $%02x", MEM[pc]);  break;
	case 0x11: sprintf_s(opstr, "ORA ($%02x),Y", MEM[pc]);  break;
	case 0x15: sprintf_s(opstr, "ORA $%02x,X", MEM[pc]);  break;
	case 0x16: sprintf_s(opstr, "ASL $%02x,X", MEM[pc]);  break;
	case 0x18: sprintf_s(opstr, "CLC"); break;
	case 0x19: sprintf_s(opstr, "ORA $%02x%02x,Y", MEM[pc + 1], MEM[pc]); break;
	case 0x1d: sprintf_s(opstr, "ORA $%02x%02x,X", MEM[pc + 1], MEM[pc]); break;
	case 0x1e: sprintf_s(opstr, "ASL $%02x%02x,X", MEM[pc + 1], MEM[pc]); break;
	case 0x20: sprintf_s(opstr, "JSR $%02x%02x", MEM[pc + 1], MEM[pc]); break;
	case 0x21: sprintf_s(opstr, "AND ($%02x,X)", MEM[pc]);  break;
	case 0x24: sprintf_s(opstr, "BIT $%02x", MEM[pc]);  break;
	case 0x25: sprintf_s(opstr, "AND $%02x", MEM[pc]);  break;
	case 0x26: sprintf_s(opstr, "ROL $%02x", MEM[pc]);  break;
	case 0x28: sprintf_s(opstr, "PLP"); break;
	case 0x29: sprintf_s(opstr, "AND #$%02x", MEM[pc]);  break;
	case 0x2a: sprintf_s(opstr, "ROL A"); break;
	case 0x2c: sprintf_s(opstr, "BIT $%02x%02x", MEM[pc + 1], MEM[pc]); break;
	case 0x2d: sprintf_s(opstr, "AND $%02x%02x", MEM[pc + 1], MEM[pc]); break;
	case 0x2e: sprintf_s(opstr, "ROL $%02x%02x", MEM[pc + 1], MEM[pc]); break;
	case 0x30: sprintf_s(opstr, "BMI $%02x", MEM[pc]);  break;
	case 0x31: sprintf_s(opstr, "AND ($%02x),Y", MEM[pc]);  break;
	case 0x35: sprintf_s(opstr, "AND $%02x,X", MEM[pc]);  break;
	case 0x36: sprintf_s(opstr, "ROL $%02x,X", MEM[pc]);  break;
	case 0x38: sprintf_s(opstr, "SEC"); break;
	case 0x39: sprintf_s(opstr, "AND $%02x%02x,Y", MEM[pc + 1], MEM[pc]); break;
	case 0x3d: sprintf_s(opstr, "AND $%02x%02x,X", MEM[pc + 1], MEM[pc]); break;
	case 0x3e: sprintf_s(opstr, "ROL $%02x%02x,X", MEM[pc + 1], MEM[pc]); break;
	case 0x40: sprintf_s(opstr, "RTI"); break;
	case 0x41: sprintf_s(opstr, "EOR ($%02x,X)", MEM[pc]);  break;
	case 0x45: sprintf_s(opstr, "EOR $%02x", MEM[pc]);  break;
	case 0x46: sprintf_s(opstr, "LSR $%02x", MEM[pc]);  break;
	case 0x48: sprintf_s(opstr, "PHA"); break;
	case 0x49: sprintf_s(opstr, "EOR #$%02x", MEM[pc]);  break;
	case 0x4a: sprintf_s(opstr, "LSR A"); break;
	case 0x4c: sprintf_s(opstr, "JMP $%02x%02x", MEM[pc + 1], MEM[pc]); break;
	case 0x4d: sprintf_s(opstr, "EOR $%02x%02x", MEM[pc + 1], MEM[pc]); break;
	case 0x4e: sprintf_s(opstr, "LSR $%02x%02x", MEM[pc + 1], MEM[pc]); break;
	case 0x50: sprintf_s(opstr, "BVC $%02x", MEM[pc]);  break;
	case 0x51: sprintf_s(opstr, "EOR ($%02x),Y", MEM[pc]);  break;
	case 0x55: sprintf_s(opstr, "EOR $%02x,X", MEM[pc]);  break;
	case 0x56: sprintf_s(opstr, "LSR $%02x,X", MEM[pc]);  break;
	case 0x58: sprintf_s(opstr, "CLI"); break;
	case 0x59: sprintf_s(opstr, "EOR $%02x%02x,Y", MEM[pc + 1], MEM[pc]); break;
	case 0x5d: sprintf_s(opstr, "EOR $%02x%02x,X", MEM[pc + 1], MEM[pc]); break;
	case 0x5e: sprintf_s(opstr, "LSR $%02x%02x,X", MEM[pc + 1], MEM[pc]); break;
	case 0x60: sprintf_s(opstr, "RTS"); break;
	case 0x61: sprintf_s(opstr, "ADC ($%02x,X)", MEM[pc]);  break;
	case 0x65: sprintf_s(opstr, "ADC $%02x", MEM[pc]);  break;
	case 0x66: sprintf_s(opstr, "ROR $%02x", MEM[pc]);  break;
	case 0x68: sprintf_s(opstr, "PLA"); break;
	case 0x69: sprintf_s(opstr, "ADC #$%02x", MEM[pc]);  break;
	case 0x6a: sprintf_s(opstr, "ROR A"); break;
	case 0x6c: sprintf_s(opstr, "JMP ($%02x%02x)", MEM[pc + 1], MEM[pc]); break;
	case 0x6d: sprintf_s(opstr, "ADC $%02x%02x", MEM[pc + 1], MEM[pc]); break;
	case 0x6e: sprintf_s(opstr, "ROR $%02x%02x,X", MEM[pc + 1], MEM[pc]); break;
	case 0x70: sprintf_s(opstr, "BVS $%02x", MEM[pc]);  break;
	case 0x71: sprintf_s(opstr, "ADC ($%02x),Y", MEM[pc]);  break;
	case 0x75: sprintf_s(opstr, "ADC $%02x,X", MEM[pc]);  break;
	case 0x76: sprintf_s(opstr, "ROR $%02x,X", MEM[pc]);  break;
	case 0x78: sprintf_s(opstr, "SEI"); break;
	case 0x79: sprintf_s(opstr, "ADC $%02x%02x,Y", MEM[pc + 1], MEM[pc]); break;
	case 0x7d: sprintf_s(opstr, "ADC $%02x%02x,X", MEM[pc + 1], MEM[pc]); break;
	case 0x7e: sprintf_s(opstr, "ROR $%02x%02x", MEM[pc + 1], MEM[pc]); break;
	case 0x81: sprintf_s(opstr, "STA ($%02x,X)", MEM[pc]);  break;
	case 0x84: sprintf_s(opstr, "STY $%02x", MEM[pc]);  break;
	case 0x85: sprintf_s(opstr, "STA $%02x", MEM[pc]);  break;
	case 0x86: sprintf_s(opstr, "STX $%02x", MEM[pc]);  break;
	case 0x88: sprintf_s(opstr, "DEY"); break;
	case 0x8a: sprintf_s(opstr, "TXA"); break;
	case 0x8c: sprintf_s(opstr, "STY $%02x%02x", MEM[pc + 1], MEM[pc]); break;
	case 0x8d: sprintf_s(opstr, "STA $%02x%02x", MEM[pc + 1], MEM[pc]); break;
	case 0x8e: sprintf_s(opstr, "STX $%02x%02x", MEM[pc + 1], MEM[pc]); break;
	case 0x90: sprintf_s(opstr, "BCC $%04x", MEM[pc]);  break; //PLUS SAVEpc
	case 0x91: sprintf_s(opstr, "STA ($%02x),Y", MEM[pc]);  break;
	case 0x94: sprintf_s(opstr, "STY $%02x,X", MEM[pc]);  break;
	case 0x95: sprintf_s(opstr, "STA $%02x,X", MEM[pc]);  break;
	case 0x96: sprintf_s(opstr, "STX $%02x,Y", MEM[pc]);  break;
	case 0x98: sprintf_s(opstr, "TYA"); break;
	case 0x99: sprintf_s(opstr, "STA $%02x%02x,Y", MEM[pc + 1], MEM[pc]); break;
	case 0x9a: sprintf_s(opstr, "TXS"); break;
	case 0x9d: sprintf_s(opstr, "STA $%02x%02x,X", MEM[pc + 1], MEM[pc]); break;
	case 0xa0: sprintf_s(opstr, "LDY #$%02x", MEM[pc]);  break;
	case 0xa1: sprintf_s(opstr, "LDA ($%02x,X)", MEM[pc]);  break;
	case 0xa2: sprintf_s(opstr, "LDX #$%02x", MEM[pc]);  break;
	case 0xa4: sprintf_s(opstr, "LDY $%02x", MEM[pc]);  break;
	case 0xa5: sprintf_s(opstr, "LDA $%02x", MEM[pc]);  break;
	case 0xa6: sprintf_s(opstr, "LDX $%02x", MEM[pc]);  break;
	case 0xa8: sprintf_s(opstr, "TAY"); break;
	case 0xa9: sprintf_s(opstr, "LDA #$%02x", MEM[pc]);  break;
	case 0xaa: sprintf_s(opstr, "TAX"); break;
	case 0xac: sprintf_s(opstr, "LDY $%02x%02x", MEM[pc + 1], MEM[pc]); break;
	case 0xad: sprintf_s(opstr, "LDA $%02x%02x", MEM[pc + 1], MEM[pc]); break;
	case 0xae: sprintf_s(opstr, "LDX $%02x%02x", MEM[pc + 1], MEM[pc]); break;
	case 0xb0: sprintf_s(opstr, "BCS $%02x", MEM[pc]);  break;
	case 0xb1: sprintf_s(opstr, "LDA ($%02x),Y", MEM[pc]);  break;
	case 0xb4: sprintf_s(opstr, "LDY $%02x,X", MEM[pc]);  break;
	case 0xb5: sprintf_s(opstr, "LDA $%02x,X", MEM[pc]);  break;
	case 0xb6: sprintf_s(opstr, "LDX $%02x,Y", MEM[pc]);  break;
	case 0xb8: sprintf_s(opstr, "CLV"); break;
	case 0xb9: sprintf_s(opstr, "LDA $%02x%02x,Y", MEM[pc + 1], MEM[pc]); break;
	case 0xba: sprintf_s(opstr, "TSX"); break;
	case 0xbc: sprintf_s(opstr, "LDY $%02x%02x,X", MEM[pc + 1], MEM[pc]); break;
	case 0xbd: sprintf_s(opstr, "LDA $%02x%02x,X", MEM[pc + 1], MEM[pc]); break;
	case 0xbe: sprintf_s(opstr, "LDX $%02x%02x,Y", MEM[pc + 1], MEM[pc]); break;
	case 0xc0: sprintf_s(opstr, "CPY #$%02x", MEM[pc]);  break;
	case 0xc1: sprintf_s(opstr, "CMP ($%02x,X)", MEM[pc]);  break;
	case 0xc4: sprintf_s(opstr, "CPY $%02x", MEM[pc]);  break;
	case 0xc5: sprintf_s(opstr, "CMP $%02x", MEM[pc]);  break;
	case 0xc6: sprintf_s(opstr, "DEC $%02x", MEM[pc]);  break;
	case 0xc8: sprintf_s(opstr, "INY"); break;
	case 0xc9: sprintf_s(opstr, "CMP #$%02x", MEM[pc]);  break;
	case 0xca: sprintf_s(opstr, "DEX"); break;
	case 0xcc: sprintf_s(opstr, "CPY $%02x%02x", MEM[pc + 1], MEM[pc]); break;
	case 0xcd: sprintf_s(opstr, "CMP $%02x%02x", MEM[pc + 1], MEM[pc]); break;
	case 0xce: sprintf_s(opstr, "DEC $%02x%02x", MEM[pc + 1], MEM[pc]); break;
	case 0xd0: sprintf_s(opstr, "BNE $%02x%02x", MEM[pc + 2], MEM[pc + 1]);  break;
	case 0xd1: sprintf_s(opstr, "CMP ($%02x),Y", MEM[pc]);  break;
	case 0xd5: sprintf_s(opstr, "CMP $%02x,X", MEM[pc]);  break;
	case 0xd6: sprintf_s(opstr, "DEC $%02x,X", MEM[pc]);  break;
	case 0xd8: sprintf_s(opstr, "CLD"); break;
	case 0xd9: sprintf_s(opstr, "CMP $%02x%02x,Y", MEM[pc + 1], MEM[pc]); break;
	case 0xdd: sprintf_s(opstr, "CMP $%02x%02x,X", MEM[pc + 1], MEM[pc]); break;
	case 0xde: sprintf_s(opstr, "DEC $%02x%02x,X", MEM[pc + 1], MEM[pc]); break;
	case 0xe0: sprintf_s(opstr, "CPX #$%02x", MEM[pc]);  break;
	case 0xe1: sprintf_s(opstr, "SBC ($%02x,X)", MEM[pc]);  break;
	case 0xe4: sprintf_s(opstr, "CPX $%02x", MEM[pc]);  break;
	case 0xe5: sprintf_s(opstr, "SBC $%02x", MEM[pc]);  break;
	case 0xe6: sprintf_s(opstr, "INC $%02x", MEM[pc]);  break;
	case 0xe8: sprintf_s(opstr, "INX"); break;
	case 0xe9: sprintf_s(opstr, "SBC #$%02x", MEM[pc]);  break;
	case 0xea: sprintf_s(opstr, "NOP"); break;
	case 0xec: sprintf_s(opstr, "CPX $%02x%02x", MEM[pc + 1], MEM[pc]); break;
	case 0xed: sprintf_s(opstr, "SBC $%02x%02x", MEM[pc + 1], MEM[pc]); break;
	case 0xee: sprintf_s(opstr, "INC $%02x%02x", MEM[pc + 1], MEM[pc]); break;
	case 0xf0: sprintf_s(opstr, "BEQ $%02x", MEM[pc]);  break;
	case 0xf1: sprintf_s(opstr, "SBC ($%02x),Y", MEM[pc]);  break;
	case 0xf5: sprintf_s(opstr, "SBC $%02x,X", MEM[pc]);  break;
	case 0xf6: sprintf_s(opstr, "INC $%02x,X", MEM[pc]);  break;
	case 0xf8: sprintf_s(opstr, "SED"); break;
	case 0xf9: sprintf_s(opstr, "SBC $%02x%02x,Y", MEM[pc + 1], MEM[pc]); break;
	case 0xfd: sprintf_s(opstr, "SBC $%02x%02x,X", MEM[pc + 1], MEM[pc]); break;
	case 0xfe: sprintf_s(opstr, "INC $%02x%02x,X", MEM[pc + 1], MEM[pc]); break;

	default: sprintf_s(opstr, "UNKNWN 65C02? OPCODE: %x", opcode); break;
	}
	rval = opstr;

	return rval;
}