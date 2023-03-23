#include "emu_vector_draw.h"
#include "colordefs.h"
//#include "vector.h"
#include "log.h"




void EmuDraw2D::blit_any_tex(GLuint *texture, int blending, float alpha, int x, int y, int w, int h)
{
	glPushMatrix();
	glLoadIdentity();
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBindTexture(GL_TEXTURE_2D, *texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);	// Linear Filtering
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);	// Linear Filtering



	switch (blending)
	{
	case 0:glBlendFunc(GL_DST_COLOR, GL_ZERO); break;
	case 1:glBlendFunc(GL_SRC_COLOR, GL_ONE); break;
	case 2:glBlendFunc(GL_SRC_ALPHA, GL_ONE); break;
	case 3:glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); break;
	case 4:glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE); break;
	case 5:glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR); break;
	case 6:glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA); break;
	case 7:glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO); break;
	case 8:glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_COLOR); break;
	case 9:glBlendFunc(GL_SRC_ALPHA, GL_DST_COLOR); break;
	case 10:glBlendFunc(GL_ONE, GL_ONE); break;
	case 11:glBlendFunc(GL_ONE, GL_ZERO); break;
	case 12:glBlendFunc(GL_ZERO, GL_ONE); break;
	case 13:glBlendFunc(GL_ONE, GL_DST_COLOR); break;
	case 14:glBlendFunc(GL_SRC_ALPHA, GL_DST_COLOR); break;
	case 15:glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_COLOR); break;
	}

	glColor4f(1.0f, 1.0f, 1.0f, alpha);
	glTranslated(x, y, 0);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 1); glVertex2i(0, h);
	glTexCoord2f(0, 0); glVertex2i(0, 0);
	glTexCoord2f(1, 0); glVertex2i(w, 0);
	glTexCoord2f(1, 1); glVertex2i(w, h);
	glEnd();
	glDisable(GL_TEXTURE_2D);
	glPopMatrix();
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}


EmuDraw2D::EmuDraw2D() //Add init here for blending color or BW from emulator code
{
	
	tex=0;
	set_blendmode(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//add calculate_texture_offsets here when ini support added!
}


EmuDraw2D::~EmuDraw2D()
{
	
	tex = 0;
}


void EmuDraw2D::set_texture_id(GLuint *id)
{
	tex = id;
}


void EmuDraw2D::set_blendmode(GLenum sfactor, GLenum dfactor)
{
	glBlendFunc(sfactor, dfactor);
}


void EmuDraw2D::calculate_texture_offsets(int screen_width, int screen_height, int shotsize)
{
	//To be added, Placeholder!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	//float xoffset = (1.0/1024.0)*15.0; //15 is shotsize variable
	//float yoffset = (1.0/870.0)*15.0; //15 is shotsize variable

}


rgb_t EmuDraw2D::cache_tex_color(int intensity, rgb_t col)
{

	UINT8 *test = (UINT8 *)&col;

	//test[0] = clip((test[0] & intensity) + config.gain, 0, 0xff);
	//test[1] = clip((test[1] & intensity) + config.gain, 0, 0xff);
	//test[2] = clip((test[2] & intensity) + config.gain, 0, 0xff);
	test[3] = 0xff;

	//Make sure that there is an alpha set in this!
	//texlist[tptr].color = 0xffffff;//col;
	return col;
}


rgb_t EmuDraw2D::cache_color(int intensity, rgb_t col)
{
	UINT8 *test = (UINT8 *)&col;

	//test[0] = clip((test[0] & intensity) + config.gain, 0, 0xff);
	//test[1] = clip((test[1] & intensity) + config.gain, 0, 0xff);
	//test[2] = clip((test[2] & intensity) + config.gain, 0, 0xff);
	test[3] = 0xff;
	
	//wrlog("col here is %x");
	return col;
}


void EmuDraw2D::cache_texpoint(float ex, float ey, float tx, float ty, int intensity, rgb_t col)
{
	//texlist[tptr].x = ex - xoffset;
	//texlist[tptr].texx = tx;
	//texlist[tptr].y = ey - yoffset;
	//texlist[tptr].texy = ty;
	//cache_tex_color(intensity, col);
	
	texlist.emplace_back(ex - xoffset, ey - yoffset, tx, ty, cache_tex_color(intensity, col));

}


void EmuDraw2D::add_tex(float sx, float sy, float ex, float ey, int intensity, rgb_t col)
{

	float xoffset = (1.0 / 1024.0)*25.0; //15 is shotsize variable
	float yoffset = (1.0 / 860.0)*25.0; //15 is shotsize variable
	//wrlog("Xoffset here is %f, Yoffset here is %f", xoffset, yoffset);
										
	xoffset = 15;
	yoffset = 15;
	/*
	//V0
	txlist.emplace_back(CurX, CurY, s, t, fcolor);
	//V1
	txlist.emplace_back(DstX, CurY, s + w, t, fcolor);
	//V2
	txlist.emplace_back(DstX, DstY, s + w, t - h, fcolor);
	//V2
	txlist.emplace_back(DstX, DstY, s + w, t - h, fcolor);
	//V3
	txlist.emplace_back(CurX, DstY, s, t - h, fcolor);
	//V0
	txlist.emplace_back(CurX, CurY, s, t, fcolor);
		*/
	
	
	cache_texpoint(ex - xoffset, ey - yoffset, 0, 0, intensity, col);
	cache_texpoint(ex + xoffset, ey - yoffset, 1, 0, intensity, col);
	cache_texpoint(ex + xoffset, ey + yoffset, 1, 1, intensity, col);
	cache_texpoint(ex - xoffset, ey + yoffset, 0, 1, intensity, col);

	//glTexCoord2f(0, 1); glVertex2i(0, h);
	//glTexCoord2f(0, 0); glVertex2i(0, 0);
	//glTexCoord2f(1, 0); glVertex2i(w, 0);
	//glTexCoord2f(1, 1); glVertex2i(w, h);

}


void EmuDraw2D::add_line(float sx, float sy, float ex, float ey, int intensity, rgb_t col)
{

	linelist.emplace_back(sx, sy, (cache_color(intensity, col)));
	linelist.emplace_back(ex, ey, (cache_color(intensity, col)));
}


void EmuDraw2D::draw_all()
{
	glDisable(GL_TEXTURE_2D);
	//Draw Lines and endpoints
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);

	glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(fpoint), &linelist[0].color);
	glVertexPointer(2, GL_FLOAT, sizeof(fpoint), &linelist[0].x);


	glDrawArrays(GL_POINTS, 0, (GLsizei)linelist.size());
	glDrawArrays(GL_LINES, 0, (GLsizei)linelist.size());

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	if (!texlist.empty()) {
		//Draw Textured Shots for Asteroids. This code is only used when running Asteroids/Asteroids Deluxe or one of it's clones.
		//wrlog("Drawing Tex at %f , %f", texlist[0].x, texlist[0].y);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, *tex);
		glBlendFunc(GL_ONE, GL_ONE);
		//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);	
		glColor4f(1.0f, 1.0f, 1.0f,1.0f);

		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(2, GL_FLOAT, sizeof(txdata), &texlist[0].x);

		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glTexCoordPointer(2, GL_FLOAT, sizeof(txdata), &texlist[0].tx);

		glEnableClientState(GL_COLOR_ARRAY);
		glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(txdata), &texlist[0].color);

		//glDrawArrays(GL_TRIANGLES, 0, (GLsizei)texlist.size());
		glDrawArrays(GL_QUADS, 0, (GLsizei)texlist.size());

		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);
		//glDisableClientState(GL_COLOR_ARRAY);

		//Change Blending back to Line Drawing
		glDisable(GL_TEXTURE_2D);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	//Reset index pointers.
	texlist.clear();
	linelist.clear();


}
