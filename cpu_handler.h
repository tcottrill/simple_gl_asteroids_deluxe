
#ifndef CPU_HANDLER_H
#define CPU_HANDLER_H

//#include <cstdint>

#ifndef _MEMORYREADWRITEBYTE_
#define _MEMORYREADWRITEBYTE_
#endif


struct MemoryWriteByte
{
	unsigned int lowAddr;
	unsigned int highAddr;
	void(*memoryCall)(unsigned int, unsigned char, struct MemoryWriteByte *);
	void *pUserArea;
};

struct MemoryReadByte
{
	unsigned int lowAddr;
	unsigned int highAddr;
	unsigned char(*memoryCall)(unsigned int, struct MemoryReadByte *);
	void *pUserArea;
};



//#define READ_HANDLER(name)  static uint8_t name(uint32_t address, struct MemoryReadByte *psMemRead)
//#define WRITE_HANDLER(name)  static void name(uint32_t address, uint8_t data, struct MemoryWriteByte *psMemWrite)
#define READ_HANDLER(name)  static UINT8 name(UINT32 address, struct MemoryReadByte *psMemRead)
#define WRITE_HANDLER(name)  static void name(UINT32 address, UINT8 data, struct MemoryWriteByte *psMemWrite)
#define MEM_WRITE(name) struct MemoryWriteByte name[] = {
#define MEM_READ(name)  struct MemoryReadByte name[] = {
#define MEM_ADDR(start,end,routine) {start,end,routine},
#define MEM_END {(UINT32) -1,(UINT32) -1,NULL}};


#endif