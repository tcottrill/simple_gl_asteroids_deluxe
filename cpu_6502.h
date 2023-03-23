
#ifndef _6502_H_
#define _6502_H_

#pragma once

//
// The core of this code comes from http://rubbermallet.org/fake6502.c (c) 2011 Mike Chambers.
//

#undef int8_t
#undef uint8_t
#undef int16_t
#undef uint16_t
#undef int32_t
#undef uint32_t
#undef intptr_t
#undef uintptr_t
#undef int64_t
#undef uint64_t



#include <cstdint>
#include <string>
#include "cpu_handler.h"
// Address mask. Atari Asteroids/Deluxe use 0x7fff -
// but use 0xffff for full 16 bit decode


class cpu_6502
{

public:
	
	enum
	{
		M6502_A = 0x01,
		M6502_X = 0x02,
		M6502_Y = 0x04,
		M6502_P = 0x08,
		M6502_S = 0x10,
	};

	// Pointer to the game memory map (32 bit)
	uint8_t *MEM = nullptr;
	//Pointer to the handler structures
	MemoryReadByte  *memory_read = nullptr;
	MemoryWriteByte *memory_write = nullptr;

	//Constructors
	cpu_6502(uint8_t *mem, MemoryReadByte *read_mem, MemoryWriteByte *write_mem, uint16_t addr, int num);
	cpu_6502() {};

	//Destructor
	~cpu_6502() {};

	// must be called first to initialise the 6502 instance, this is called in the primary constructor
	void init6502(uint16_t addrmaskval);
	//Irq Handler
	void irq6502();
	//Int Handler
	void nmi6502();
	// Sets all of the 6502 registers. 
	void reset6502();
	// Run the 6502 engine for specified number of clock cycles 
	int  exec6502(int timerTicks);
	//Step the cpu 1 instruction
	int  step6502();
	//Get elapsed ticks / reset to zero
	int  get6502ticks(int reset);
	//Get Register values
	uint8_t get_reg(int regnum);
	//Set Register Values
	void m6502_set_reg(int regnum, uint8_t val);
	//Get the PC
	uint16_t get_pc();
	//Force a jump to a different PC
	void set_pc(uint16_t _pc);
	//Get the previous PC
	uint16_t get_ppc();
	//Return the string value of the last instruction
	std::string disam(uint8_t opcode);
	//Use Mame style memory handling, block read/writes that don't go through the handlers.
	void mame_memory_handling(bool s) { mmem = s; }
	void log_unhandled_rw(bool s) { log_debug_rw = s; }
	
private:

	#define FLAG_CARRY     0x01
	#define FLAG_ZERO      0x02
	#define FLAG_INTERRUPT 0x04
	#define FLAG_DECIMAL   0x08
	#define FLAG_BREAK     0x10
	#define FLAG_CONSTANT  0x20
	#define FLAG_OVERFLOW  0x40
	#define FLAG_SIGN      0x80

	#define BASE_STACK     0x100

	#define saveaccum(n) a = (uint8_t)((n) & 0x00FF)


	//flag modifier macros
	#define setcarry() status |= FLAG_CARRY
	#define clearcarry() status &= (~FLAG_CARRY)
	#define setzero() status |= FLAG_ZERO
	#define clearzero() status &= (~FLAG_ZERO)
	#define setinterrupt() status |= FLAG_INTERRUPT
	#define clearinterrupt() status &= (~FLAG_INTERRUPT)
	#define setdecimal() status |= FLAG_DECIMAL
	#define cleardecimal() status &= (~FLAG_DECIMAL)
	#define setoverflow() status |= FLAG_OVERFLOW
	#define clearoverflow() status &= (~FLAG_OVERFLOW)
	#define setsign() status |= FLAG_SIGN
	#define clearsign() status &= (~FLAG_SIGN)


	//flag calculation macros
	#define zerocalc(n) { if ((n) & 0x00FF) clearzero(); else setzero(); }
	#define signcalc(n) { if ((n) & 0x0080) setsign(); else clearsign(); }
	#define carrycalc(n) { if ((n) & 0xFF00) setcarry(); else clearcarry(); }
	#define overflowcalc(n, m, o) { if (((n) ^ (uint16_t)(m)) & ((n) ^ (o)) & 0x0080) setoverflow(); else clearoverflow(); }

	//Memory handlers
	uint8_t read6502(uint16_t addr);
	void    write6502(uint16_t addr, uint8_t byte);

	// Function pointer arrays (static) 
	void (cpu_6502::*adrmode[0x100])();
	void (cpu_6502::*instruction[0x100])();
	//Data movers 
	void	 push16(uint16_t pushval);
	void	 push8(uint8_t pushval);
	uint16_t pull16();
	uint8_t  pull8();
	uint16_t getvalue();
	uint16_t getvalue16();
	void     putvalue(uint16_t saveval);
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	//Address Mask for < 0xffff top
	uint16_t addrmask;
		
	//6502 CPU registers
	uint16_t pc;
	uint8_t sp, a, x, y, status;
	//helper variables
	uint8_t penaltyop, penaltyaddr;
	uint32_t instructions = 0; //keep track of total instructions executed
	int32_t clockticks6502 = 0, clockgoal6502 = 0; //Clockgoal no longer needed?
	uint16_t oldpc, ea, reladdr, value, result;
	uint8_t opcode, oldstatus, useaccum;
	uint16_t ppc;
	int clocktickstotal; //Runnning, resetable total of clockticks
	bool debug = 0;
	bool mmem = 0; //Use mame style memory handling, reject unhandled read/writes
	bool log_debug_rw = 0; //Log unhandled reads and writes
	bool irg_pending = 0;
	//For multicpu use
	int cpu_num;
	//for testing only.
	int num;
	
	// 6502 Addressing mode functions, calculates effective addresses
	void imp();
	void acc();
	void imm();
	void zp();
	void zpx();
	void zpy();
	void rel();
	void abso();
	void absx();
	void absy();
	void ind();
	void indx();
	void indy();

	//6502 Instructions.
	void adc();
	void op_and(); 
	void asl();
	void bcc();
	void bcs();
	void beq();
	void op_bit();
	void bmi();
	void bne();
	void bpl();
	void brk();
	void bvc();
	void bvs();
	void clc();
	void cld();
	void cli();
	void clv();
	void cmp();
	void cpx();
	void cpy();
	void dec();
	void dex();
	void dey();
	void eor();
	void inc();
	void inx();
	void iny();
	void jmp();
	void jsr();
	void lda();
	void ldx();
	void ldy();
	void lsr();
	void nop();
	void ora();
	void pha();
	void php();
	void pla();
	void plp();
	void rol();
	void ror();
	void rti();
	void rts();
	void sbc();
	void sec();
	void sed();
	void sei();
	void sta();
	void stx();
	void sty();
	void tax();
	void tay();
	void tsx();
	void txa();
	void txs();
	void tya();
	//undocumented instructions
	void lax();
	void sax();
	void dcp();
	void isb();
	void slo();
	void rla();
	void sre();
	void rra();
};

#endif 

