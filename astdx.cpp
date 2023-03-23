/* Asteroid Deluxe Emu */

// 
// NOTE: A lot of the code below was taken from very old versions of MAME(TM) 
// Some code below is also from Retrocade, some is my own work. 
//

/*
ROM_START( astdelux )
ROM_REGION( 0x8000, "maincpu", 0 )
ROM_LOAD( "036430-02.d1",  0x6000, 0x0800, CRC(a4d7a525) SHA1(abe262193ec8e1981be36928e9a89a8ac95cd0ad) )
ROM_LOAD( "036431-02.ef1", 0x6800, 0x0800, CRC(d4004aae) SHA1(aa2099b8fc62a79879efeea70ea1e9ed77e3e6f0) )
ROM_LOAD( "036432-02.fh1", 0x7000, 0x0800, CRC(6d720c41) SHA1(198218cd2f43f8b83e4463b1f3a8aa49da5015e4) )
ROM_LOAD( "036433-03.j1",  0x7800, 0x0800, CRC(0dcc0be6) SHA1(bf10ffb0c4870e777d6b509cbede35db8bb6b0b8) )
// Vector ROM
ROM_LOAD( "036800-02.r2",  0x4800, 0x0800, CRC(bb8cabe1) SHA1(cebaa1b91b96e8b80f2b2c17c6fd31fa9f156386) )
ROM_LOAD( "036799-01.np2", 0x5000, 0x0800, CRC(7d511572) SHA1(1956a12bccb5d3a84ce0c1cc10c6ad7f64e30b40) )

// DVG PROM
ROM_REGION( 0x100, "dvg:prom", 0 )
ROM_LOAD( "034602-01.c8",  0x0000, 0x0100, CRC(97953db8) SHA1(8cbded64d1dd35b18c4d5cece00f77e7b2cab2ad) )
ROM_REGION( 0x40, "earom", ROMREGION_ERASE00 ) // default to zero fill to suppress invalid high score display

*/


#include <windows.h>
#include "glew.h"
#include "wglew.h"
#include "log.h"
#include "winfont.h"
#include "rawinput.h"
#include "fileio.h"
#include "ini.h"
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <process.h>
#include <memory.h>
#include "cpu_6502.h"
#include "pokey.h"
#include "earom.h"
#include "astdx.h"
#include "mixer.h"
#include "colordefs.h"
#include "simple_texture.h"
#include "emu_vector_draw.h"
#include "newfbo.h"
#include "texrect2dvbo.h"

multifbo* fbo;
unsigned char* GI = NULL;

cpu_6502* CPU;
TEX* shot;
EmuDraw2D* emuscreen;
Rect2DVBO* screenRect2 = nullptr;
enum RotationDir { NORMAL, RIGHT, LEFT, FLIP } rotation;

static int sample_pos = 0;

void calc_screen_rect(int screen_width, int screen_height, char* aspect, int rotated)
{
	float indices[32] =
	{
		//normal
		0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0, 0.0,
		//rotated right
		1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0,
		//rotated left
		0.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0,
		//flip
		1.0, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0
	};

	float target_width = 0.0;
	float new_screen_height = 0.0;
	float xadj = 0.0;
	float yadj = 0.0;
	int temp = 0;

	//Lets decode our aspect first!
	int aspectx, aspecty;

	if (sscanf(aspect, "%d:%d", &aspectx, &aspecty) != 2 || aspectx == 0 || aspecty == 0)
	{
		wrlog("error: invalid value for aspect ratio: %s\n", aspect);
		wrlog("Setting aspect to 4/3 because of invalid config file setting.");
		aspectx = 4; aspecty = 3;
	}

	//rotated = RotationDir::RIGHT;
	if (rotated)
	{
		temp = aspecty;
		aspecty = aspectx;
		aspectx = temp;
	}

	//First, lets see if it will fit on the screen
	target_width = (float)screen_height * (float)((float)aspectx / (float)aspecty);

	if (target_width > screen_width)
	{
		//Oh no, now we have to adjust the Y value.
		new_screen_height = (float)screen_width * (float)((float)aspecty / (float)aspectx);
		wrlog("New Screen Height is %f", new_screen_height);
		//Get the new width value that should fit on the screen
		target_width = (float)new_screen_height * (float)((float)aspectx / (float)aspecty);
		yadj = ((float)screen_height - (float)new_screen_height) / 2;
		wrlog("Yadj (Divided by 2) is %f", yadj);
		screen_height = new_screen_height;
	}
	wrlog("Target width is %f", target_width);

	//Now adjust to center in the screen
	xadj = (screen_width - target_width) / 2.0;
	wrlog("Xadj (Divided by 2) is %f", xadj);


	int v = 8 * rotated; //0,16,24
	//Now set the points

	screenRect2->BottomLeft(xadj, yadj, indices[v], indices[v + 1]);
	screenRect2->TopLeft(xadj, screen_height + yadj, indices[v + 2], indices[v + 3]);
	screenRect2->TopRight(target_width + xadj, screen_height + yadj, indices[v + 4], indices[v + 5]);
	screenRect2->BottomRight(target_width + xadj, yadj, indices[v + 6], indices[v + 7]);
}

struct roms
{
	char* filename;
	UINT16 loadAddr;
	UINT16 romSize;
};

struct roms images[] =
{
	//Vector Roms
	{"roms\\astdelux\\036800-02.r2", 0x4800, 0x800},
	{"roms\\astdelux\\036799-01.np2", 0x5000, 0x800},
	//Main Roms
	{"roms\\astdelux\\036430-02.d1", 0x6000, 0x800},
	{"roms\\astdelux\\036431-02.ef1", 0x6800, 0x800},
	{"roms\\astdelux\\036432-02.fh1", 0x7000, 0x800},
	{"roms\\astdelux\\036433-03.j1", 0x7800, 0x800},
	{"roms\\astdelux\\036433-03.j1", 0xf800, 0x800},
	// Not using DVG Rom
	{NULL, 0, 0}
};

//Audio
#define BUFFER_SIZE 44100/60
int thrust_voice;
int lastret;
short int* ast_soundbuffer = NULL;
int thrust;
int explode1;
int explode2;
int explode3;


//CONFIG KEYS
float gamma;
int freeplay;
int lives;
int language;
int romset;
float linewidth;
float pointsize;
int cocktail;
int translucent;
int bonuslevel;
int difficulty;
int pokeyvol;
int explodevol;
int thrustvol;
int colordepth;
int minimumplays;


UINT8 swapval = 0;
UINT8 lastswap = 0;
UINT8 buffer[0x100];



int scale_by_cycles(int val, int clock)
{
	int k;
	int current = 0;
	int max;

	current = CPU->get6502ticks(0);
	max = clock / 60;

	k = val * (float)((float)current / (float)max);

	return k;
}


static void pokey_update()
{
	int newpos = 0;

	newpos = scale_by_cycles(BUFFER_SIZE, 1512000);

	if (newpos - sample_pos < 10) 	return;
	Pokey_process(ast_soundbuffer + sample_pos, newpos - sample_pos);
	sample_pos = newpos;
}


void pokey_do_update()
{
	if (sample_pos < BUFFER_SIZE) { Pokey_process(ast_soundbuffer + sample_pos, BUFFER_SIZE - sample_pos); }
	sample_pos = 0;
}


#define twos_comp_val(num,bits) ((num&(1<<(bits-1)))?(num|~((1<<bits)-1)):(num&((1<<bits)-1)))
#define VEC_SHIFT 16

// I'm not using the current, more accurate vector code from MAME in this emulator. I have used it in a modern, unreleased version of AAE. 
// I don't particularly like it, on my real asteroids monitor the analog aliasing looks closer to this old code. The newer code is missing the analog smoothing emulation.

void dvg_generate_vector_list()
{
	unsigned int part1;
	unsigned int part2;
	int  pc = 0x4000;
	int sp = 0;
	int stack[4];
	int scale = 0;
	int currentx, currenty = 0;
	int done = 0;
	unsigned int firstwd, secondwd = 0;
	unsigned int opcode;
	int  x, y;
	int temp;
	int z;
	int a;
	int deltax, deltay;


	while (!done)
	{
		part1 = GI[pc] & 0xff;
		part2 = GI[pc + 1] & 0xff;
		firstwd = (((part1) | (part2 << 8)));
		opcode = firstwd & 0xf000;
		pc++;
		pc++;

		if ((opcode >= 0x1000) && (opcode <= 0xa000))
		{
			part1 = GI[pc] & 0xff;
			part2 = GI[pc + 1] & 0xff;
			secondwd = ((part1) | (part2 << 8));
			pc++;
			pc++;
		}
		switch (opcode)
		{
		case 0: {done = 1;  break; }

		case 0xf000:

			// compute raw X and Y values //
			z = (firstwd & 0xf0) >> 4;
			y = firstwd & 0x0300;
			x = (firstwd & 0x03) << 8;
			//Check Sign Values and adjust as necessary
			if (firstwd & 0x0400) { y = -y; }
			if (firstwd & 0x04) { x = -x; }
			//Invert Drawing if in Cocktal mode and Player 2 selected
			if (GI[0x1e] != 0 && cocktail) { x = -x; y = -y; }
			temp = 2 + ((firstwd >> 2) & 0x02) + ((firstwd >> 11) & 0x01);
			temp = ((scale + temp) & 0x0f);
			if (temp > 9)	temp = -1;
			deltax = (x << VEC_SHIFT) >> (9 - temp);
			deltay = (y << VEC_SHIFT) >> (9 - temp);
			goto DRAWLOOP;
			break;

		case 0x1000:
		case 0x2000:
		case 0x3000:
		case 0x4000:
		case 0x5000:
		case 0x6000:
		case 0x7000:
		case 0x8000:
		case 0x9000:

			// compute raw X and Y values //
			z = secondwd >> 12;
			y = firstwd & 0x03ff;
			x = secondwd & 0x03ff;

			//Check Sign Values and adjust as necessary
			if (firstwd & 0x0400) { y = -y; }
			if (secondwd & 0x400) { x = -x; }
			//Invert Drawing if in Cocktail mode and Player 2 selected
			if (GI[0x1e] != 0 && cocktail)
			{
				x = -x;	y = -y;
			}
			// Do overall scaling
			temp = scale + (opcode >> 12);
			temp = temp & 0x0f;

			if (temp > 9) { temp = -1; }

			deltax = (x << VEC_SHIFT) >> (9 - temp);
			deltay = (y << VEC_SHIFT) >> (9 - temp);

		DRAWLOOP:
			if (z)
			{
				//wrlog("z here is %d", z);



			/*
			   switch (z)
			   {
			   case 2:  glColor4f(.3f, .3f, .3f, .95f); break;
			   case 3:  glColor4f(.5f, .5f, .5f, .95f); break;
			   case 4:  glColor4f(.6f, .6f, .6f, .95f); break;
			   case 6:  glColor4f(.6f, .6f, .6f, .95f); break;
			   case 7:  glColor4f(.7f, .7f, .7f, .95f); break;
			   case 8:  glColor4f(.75f, .75f, .75f, .95f); break;
			   case 9:  glColor4f(.8f, .8f, .8f, .95f); break; //
			   case 10: glColor4f(.85f, .85f, .85f, .95f); break; //Half the saucer+shiel
			   case 11: glColor4f(.90f, .90f, .90f, .95f); break; //
			   case 12: glColor4f(1.0f, 1.0f, 1.0f, .95f); break;
			   case 13: glColor4f(1.0f, 1.0f, 1.0f, .95f); break; //Default Ship Color
			   case 14: glColor4f(1.0f, 1.0f, 1.0f, .95f); break; //Shield
			   case 15: glColor4f(1.0f, 1.0f, 1.0f, .95f); break; //Shield

			   default: glColor4f(0.5f, 1.0f, .2f, .95f); break;
			   }
			   */


				if ((currentx == currentx + deltax) && (currenty == currenty - deltay))
				{
					//wrlog("This is a dot?");
					//glBegin(GL_POINTS);
					//glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
					//glVertex2f((currentx >> VEC_SHIFT), (currenty >> VEC_SHIFT));
					//glEnd();
					if (z != 4)
					{
						z = (z << 4) | 0x0f;
						emuscreen->add_tex(currentx >> VEC_SHIFT, currenty >> VEC_SHIFT, currentx >> VEC_SHIFT, currenty >> VEC_SHIFT, MAKE_RGB(z, z, z), MAKE_RGB(z, z, z));
					}
					else
					{
						z = (z << 4) | 0x0f;
						emuscreen->add_line(currentx >> VEC_SHIFT, currenty >> VEC_SHIFT, (currentx + deltax) >> VEC_SHIFT, (currenty - deltay) >> VEC_SHIFT, MAKE_RGB(z, z, z), MAKE_RGB(z, z, z));
					}

				}
				else
				{
					z = (z << 4) | 0x0f;
					emuscreen->add_line(currentx >> VEC_SHIFT, currenty >> VEC_SHIFT, (currentx + deltax) >> VEC_SHIFT, (currenty - deltay) >> VEC_SHIFT, MAKE_RGB(z, z, z), MAKE_RGB(z, z, z));
				}
				/*
					glBegin(GL_LINES);
					glVertex3i((currentx >> VEC_SHIFT), ((currenty) >> VEC_SHIFT), 0);					// Top Left
					glVertex3i(((currentx + deltax) >> VEC_SHIFT), ((currenty - deltay) >> VEC_SHIFT), 0);
					glEnd();

					glBegin(GL_POINTS);
					glVertex3i((currentx >> VEC_SHIFT), ((currenty) >> VEC_SHIFT), 0);					// Top Left
					glVertex3i(((currentx + deltax) >> VEC_SHIFT), ((currenty - deltay) >> VEC_SHIFT), 0);
					glEnd();
					*/


			}
			currentx += deltax;
			currenty -= deltay;
			deltax, deltay = 0;
			break;

		case 0xa000:
			x = twos_comp_val(secondwd, 12);
			y = twos_comp_val(firstwd, 12);

			//Invert the screen drawing if cocktail and Player 2 selected
			if (GI[0x1e] != 0 && cocktail)
			{
				x = 1024 - x;
				y = 1024 - y;
			}
			//Do overall draw scaling
			scale = (secondwd >> 12) & 0x0f;
			currenty = (926 - y) << VEC_SHIFT;
			currentx = (x - 32) << VEC_SHIFT;
			break;

		case 0xb000:
			done = 1;
			break;

		case 0xc000:
			a = 0x4000 + ((firstwd & 0x0fff) * 2);
			stack[sp] = pc;
			if (sp == 4)
			{
				done = 1;
				sp = 0;
			}
			else
				sp = sp + 1;
			pc = a;
			break;

		case 0xd000:
			if (sp == 0)
			{
				done = 1;
				sp = 0;
				break;
			}
			sp = sp - 1;
			pc = stack[sp];
			break;

		case 0xe000:
			temp = firstwd & 0xfff;
			if (temp == 0)
			{
				done = 1;

				break;
			}
			a = 0x4000 + ((firstwd & 0x0fff) * 2);
			pc = a;
			break;
		}
	}
}



void BWVectorGeneratorInternal(UINT32 address, UINT8 data, struct MemoryWriteByte* psMemWrite)
{

	glClear(GL_COLOR_BUFFER_BIT);// | GL_DEPTH_BUFFER_BIT);



	fbo->Use();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glDisable(GL_TEXTURE_2D);
	// Type Of Blending To Use
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


	dvg_generate_vector_list();
	emuscreen->set_texture_id(&shot->texid);
	emuscreen->draw_all();
	fbo->UnUse();
	fbo->Bind(0, 0);


	glEnable(GL_TEXTURE_2D);

	//screenRect2->Render();

	glBegin(GL_QUADS);
	//glTexCoord2f(0, 1); glVertex2f(0, 868);
	//glTexCoord2f(0, 0); glVertex2f(0, 0);
	//glTexCoord2f(1, 0); glVertex2f(SCREEN_W, 0); 
	//glTexCoord2f(1, 1); glVertex2f(SCREEN_W, 868); 
	glTexCoord2f(0, 0); glVertex2i(0, 868);
	glTexCoord2f(0, 1); glVertex2i(0, 0);
	glTexCoord2f(1, 1); glVertex2i(1024, 0);
	glTexCoord2f(1, 0); glVertex2i(1024, 868);
	glEnd();


	//wrlog("EOF-------------------->");
	swap_buffers();
}

void AsteroidsSwapRam(UINT32 address, UINT8 data, struct MemoryWriteByte* psMemWrite)
{
	if ((swapval & 80) != lastswap)
	{
		lastswap = swapval & 0x80;
		memcpy(buffer, GI + 0x200, 0x100);
		memcpy(GI + 0x200, GI + 0x300, 0x100);
		memcpy(GI + 0x300, buffer, 0x100);
	}
}

UINT8 RandRead(UINT32 address, struct MemoryReadByte* psMemRead)
{
	int val;
	val = rand();
	return val;
}
void NoWrite(UINT32 address, UINT8 data, struct MemoryWriteByte* psMemWrite)
{}



void Thrust(UINT32 address, UINT8 data, struct MemoryWriteByte* psMemWrite)
{
	static int lastthrust = 0;
	if (!(data & 0x80) && (lastthrust & 0x80))
	{
		sample_end(4);
	}
	if ((data & 0x80) && !(lastthrust & 0x80))
	{
		sample_start(4, 0, 1);
	}
	lastthrust = data;
}

void Explosions(UINT32 address, UINT8 data, struct MemoryWriteByte* psMemWrite)
{
	if (data == 0x3d)
	{
		sample_start(1, 1, 0);
	}
	if (data == 0xfd)
	{
		sample_start(2, 2, 0);
	}
	if (data == 0xbd)
	{
		sample_start(3, 3, 0);
	}
}

void PokeyWrite(UINT32 address, UINT8 data, struct MemoryWriteByte* psMemWrite)
{
	Update_pokey_sound(address, data, 0, 6);
	pokey_update();
}


void do_sound()
{
	pokey_do_update();
	stream_update(11, 0, ast_soundbuffer);
	mixer_update();

}


void astdelux_run()
{
	// Lame, I know. 
	//dwElapsedTicks = m6502zpGetElapsedTicks(0xff);
	CPU->get6502ticks(1);
	CPU->exec6502(6200);
	CPU->nmi6502();
	CPU->exec6502(6200);
	CPU->nmi6502();
	CPU->exec6502(6200);
	CPU->nmi6502();
	CPU->exec6502(6200);
	CPU->nmi6502();
	do_sound();
}


UINT8 AstPIA1Read(UINT32 address, struct MemoryReadByte* psMemRead)
{
	switch (address)
	{
	case 0x1:
		if (lastret == 1) { lastret = 0; return 0x80; }
		else { lastret = 1; return 0x7f; }
		break;
	case 0x3: /*Shield */
		if (key[KEY_SPACE]) 	return 0x80;
		break;
	case 0x4: /* Fire */
		if (key[KEY_LCONTROL])	return 0x80;
		break;
	case 0x7: /* Self Test */
		if (key[KEY_9])		return 0x80;
		return 0;
	}
	return 0;
}


UINT8 AstPIA2Read(UINT32 address, struct MemoryReadByte* psMemRead)
{
	switch (address)
	{
	case 0x0: /* Coin in */
		if (key[KEY_5]) { return 0x80; break; }
		break;
	case 0x3: /* 1 Player start */
		if (key[KEY_1]) { return 0x80; break; }
		break;
	case 0x4: /* 2 Player start */
		if (key[KEY_2]) { return 0x80; break; }
		break;
	case 0x5: if (key[KEY_ALT]) { return 0x80; break; }
			break;
	case 0x6: /* Rotate right */
		if (key[KEY_RIGHT]) { return 0x80; break; }
		break;
	case 0x7: /* Rotate left */
		if (key[KEY_LEFT]) { return 0x80; break; }
		break;
	}
	return 0;
}

struct MemoryWriteByte AsteroidDeluxeWrite[] =
{
	{ 0x2c00, 0x2c0f, PokeyWrite },
	{ 0x3000, 0x3000, BWVectorGeneratorInternal },
	{ 0x3c03, 0x3c03, Thrust },
	{ 0x3c04, 0x3c04, AsteroidsSwapRam },
	{ 0x3600, 0x3600, Explosions },
	{ 0x3200, 0x323f, EaromWrite },
	{ 0x3a00, 0x3a00, EaromCtrl },
	{ 0x4800, 0x5fff, NoWrite },
	{ 0x6000, 0x7fff, NoWrite },
	{ (UINT32)-1,	(UINT32)-1,		NULL }
};

struct MemoryReadByte AsteroidDeluxeRead[] =
{
	{ 0x2c40, 0x2c7f, EaromRead },
	{ 0x2c0a, 0x2c0a, RandRead },
	{ 0x2400, 0x2407, AstPIA2Read },
	{ 0x2000, 0x2007, AstPIA1Read },
	{ (UINT32)-1,	(UINT32)-1,		NULL }
};


void astdelux_init()
{
	FILE* fp;
	UINT32 loop;


	glViewport(0, 0, SCREEN_W, SCREEN_H);						// Reset The Current Viewport
	glMatrixMode(GL_PROJECTION);							// Select The Projection Matrix
	glLoadIdentity();										// Reset The Projection Matrix
	glOrtho(0.0f, 1024, 0.0f, 868, -1.0f, 1.0f);		// Create Ortho 640x480 View (0,0 At Top Left)
	glMatrixMode(GL_MODELVIEW);								// Select The Modelview Matrix
	glLoadIdentity();
	glShadeModel(GL_SMOOTH);								// Enable Smooth Shading
	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);					// Black Background

	glEnable(GL_BLEND);										// Enable Blending
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_POINT_SMOOTH);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

	// Type Of Blending To Use
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glLineWidth(2.5f);
	glPointSize(1.5f);
	//glTranslatef(0.375f, 0.375f, .375f);
	wglSwapIntervalEXT(1);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	//Sound Inits

	ast_soundbuffer = (short*)malloc(BUFFER_SIZE * 2);
	memset(ast_soundbuffer, 0, BUFFER_SIZE * 2);

	//	stream = play_audio_stream(BUFFER_SIZE, 8, FALSE, 44100, pokeyvol, 128);
	Pokey_sound_init(1512000, 44100, 1); // INIT POKEY
	fbo = new multifbo(1024, 868, 16, 0, fboFilter::FB_LINEAR);

	mixer_init(44100, 60); // Init Audio Mixer
	thrust = load_sample(0, "samples\\thrust.wav");
	explode1 = load_sample(0, "samples\\explode1.wav");
	explode2 = load_sample(0, "samples\\explode2.wav");
	explode3 = load_sample(0, "samples\\explode3.wav");
	thrust = load_sample(0, "samples\\thrust.wav");

	stream_start(11, 0); //Init Stream (Always after loading any samples)

	GI = (unsigned char*)malloc(0x10000);


	for (loop = 0; loop < 0x10000; loop++)
		GI[loop] = (char)0x00;

	loop = 0;
	//Load ROMS
	while (images[loop].filename)
	{
		fp = fopen(images[loop].filename, "rb");

		if (fp == NULL)
		{

			wrlog("Cannot Locate Version Rom images!\n Are they in the roms directory?");
			exit(1);
		}
		fread(GI + images[loop].loadAddr, 1, images[loop].romSize, fp);
		fclose(fp);
		++loop;
	}

	CPU = new cpu_6502(GI, AsteroidDeluxeRead, AsteroidDeluxeWrite, 0x7fff, 0);
	CPU->reset6502();
	shot = LoadPNG("shot.png");
	//set_tex(&shot->texid);

	emuscreen = new EmuDraw2D();
	screenRect2 = new Rect2DVBO;
	emuscreen->set_texture_id(&shot->texid);

	calc_screen_rect(1024, 768, "4:3", 0);

	//Required
	GI[0x2000] = (char)0x7f;
	//DIPS

	switch (bonuslevel)
	{
		//fd 2 ships /fc 3 ships /fe 4 ff 5
	case 0:  GI[0x2800] = (char)0xff; break;
	case 10000:  GI[0x2800] = (char)0xfc; break;
	case 12000:  GI[0x2800] = (char)0xfd; break;
	case 15000:  GI[0x2800] = (char)0xfe; break;
	default: GI[0x2800] = (char)0xfd; break;
	}
	//fd bonus 12000 fc bonus 10000 fe 15000 ff no bonus
	if (difficulty && minimumplays)
	{
		GI[0x2801] = (char)0xfc;
	}//ff easy other hard plus minimum play

	if (difficulty == 0 && minimumplays)
	{
		GI[0x2801] = (char)0xfe;
	}

	if (difficulty && minimumplays == 2)
	{
		GI[0x2801] = (char)0xfd;
	}

	if (difficulty == 0 && minimumplays == 2)
	{
		GI[0x2801] = (char)0xff;
	}

	switch (lives)
	{
		//fd 2 ships /fc 3 ships /fe 4 ff 5
	case 2:  GI[0x2802] = (char)0xfc; break;
	case 3:  GI[0x2802] = (char)0xfd; break;
	case 4:  GI[0x2802] = (char)0xfe; break;
	case 5:  GI[0x2802] = (char)0xff; break;
	default: GI[0x2802] = (char)0xfd; break;
	}

	switch (language)
	{

	case 0:  GI[0x2803] = (char)0xfc; break;
	case 1:  GI[0x2803] = (char)0xfd; break;
	case 2:  GI[0x2803] = (char)0xfe; break;
	case 3:  GI[0x2803] = (char)0xff; break;
	default: GI[0x2803] = (char)0xfc; break;
	}

	if (freeplay)
	{
		GI[0x2c08] = (char)0xfb;
	} //Coinage fe 1Coin/2 credits fc 2coins/1 credit fb freeplay fd 1coin/1play
	else
	{
		GI[0x2c08] = (char)0xfd;
	}
	LoadEarom();
}

void astdelux_end()
{
	SaveEarom();
	stream_stop(11, 0);
	mixer_end();
	free(GI);
	delete emuscreen;
}



