
//#include "cpu_6502.h"
#include <stdio.h>
#include <stdlib.h>

#include "earom.h"

static int earom_offset;
static int earom_data;
static int earom[0x3f];


unsigned char EaromRead(unsigned int  address, struct MemoryReadByte *psMemRead)
{
	return (earom_data);
}

void EaromWrite(unsigned int  address, unsigned char data, struct MemoryWriteByte *psMemWrite)
{

	earom_offset = (address & 0x00ff);
	earom_data = data;
}

/* 0,8 and 14 get written to this location, too.
 * Don't know what they do exactly
 */
void EaromCtrl(unsigned int  address, unsigned char data, struct MemoryWriteByte *psMemWrite)
{

	/*
		0x01 = clock
		0x02 = set data latch? - writes only (not always)
		0x04 = write mode? - writes only
		0x08 = set addr latch?
	*/
	if (data & 0x01)
		earom_data = earom[earom_offset];
	if ((data & 0x0c) == 0x0c)
	{
		earom[earom_offset] = earom_data;

	}
}


void LoadEarom()
{

	FILE *fp;
	char c;
	int i = 0;
	fp = fopen("hi//earom.dat", "r");

	if (fp == NULL) { ; }// allegro_message("I couldn't open earom.dat for reading.\n");
	else {
		do {
			c = getc(fp);    /* get one character from the file */
			earom[i] = c;         /* display it on the monitor       */
			i++;
		} while (i < 62);    /* repeat until EOF (end of file)  */
	}
	fclose(fp);

}

void SaveEarom(void)
{
	FILE *fp;
	int i;

	fp = fopen("hi\\earom.dat", "w");
	if (fp == NULL) {
		//allegro_message("I couldn't open earom.dat for writing.\n");
	}

	for (i = 0; i < 62; i++) { fprintf(fp, "%c", earom[i]); }

	fclose(fp);
}
