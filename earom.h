
void EaromWrite(unsigned int address, unsigned char data, struct MemoryWriteByte *psMemWrite);
void EaromCtrl(unsigned int  address, unsigned char data, struct MemoryWriteByte *psMemWrite);
unsigned char EaromRead (unsigned int  address, struct MemoryReadByte *psMemRead);
void LoadEarom(void);
void SaveEarom(void);

