/*-----------------------------------------------------------------------------

  VA.cpp

-------------------------------------------------------------------------------


-----------------------------------------------------------------------------*/

#include "glew.h"
#include "wglew.h"
#include "va2dnew.h"
#include "log.h"


VA::VA(int mtype)
{
	this->points = 0;
	this->scale = 1.0f;
	this->use_textures = 0;
	this->texverts = 0;
	VA::SetType(mtype);
	wrlog("VA Create, type %x", type);

}

VA::~VA()
{
	vertices.clear();
	texvertices.clear();
}


void VA::SetType(int ntype)
{

	switch (ntype)
	{
	case 0: this->type = GL_POINTS;
		break;
	case 1: this->type = GL_LINES;
		break;
	case 2: this->type = GL_QUADS;
		type = GL_TRIANGLES;
		break;
	case 3: this->type = GL_QUADS;
		type = GL_TRIANGLES;
		use_textures = 1;
		break;
	default: wrlog("Error in VA type setting");
		this->type = GL_POINTS;
		break;
	}
};



void VA::Add2DTexVert(float x, float y, rgb_t color)
{
	tvertex.x = x;
	tvertex.y = y;

	this->texvertices.push_back(tvertex);
	this->texverts++;
}


void VA::Add2DPoint(float x, float y, rgb_t color)
{
	vertex.x = x;
	vertex.y = y;
	vertex.colors = color;
	this->vertices.push_back(vertex);
	this->points++;
}

void VA::Add2DQuad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, rgb_t color)
{
	//Vertex2D svertex;

	//Top Right    V0
	Add2DPoint(x0, y0, color);
	//Top Left     V1
	Add2DPoint(x1, y1, color);
	//Bottom Left  V2
	Add2DPoint(x2, y2, color);

	//Bottom Left  V2
	Add2DPoint(x2, y2, color);
	//Bottom Right V3
	Add2DPoint(x3, y3, color);
	//Top Right    V0
	Add2DPoint(x0, y0, color);
	
	//Bottom Right V3
	Add2DPoint(x3, y3, color);
}



void VA::Add2DScaledQuad(float x, float y, rgb_t color)
{
	Vertex2D svertex;

	x = x*scale, y = y*scale;

	//Top Right    V0
	Add2DPoint(x + scale, y + scale, color);
	//Top Left     V1
	Add2DPoint(x + scale, y, color);
	//Bottom Left  V2
	Add2DPoint(x, y, color);
	//Bottom Left  V2
	Add2DPoint(x, y, color);
	//Bottom Right V3
	Add2DPoint(x, y + scale, color);
	//Top Right    V0
	Add2DPoint(x + scale, y + scale, color);
	//Bottom Right V3
	Add2DPoint(x, y + scale, color);
}


void VA::Add2DCenteredTex(float sx, float sy, float ex, float ey, int intensity, rgb_t color)
{

	float xoffset = (1.0f / 1024.0f)*15.0f; //15 is shotsize variable Add get scale and screen size from Config Later 
	float yoffset = (1.0f / 870.0f)*15.0f; //15 is shotsize variable


   //Top Right    V0
	Add2DPoint(ex - xoffset, ey - yoffset, color);
	Add2DTexVert(0.0f, 0.0f, color);
	//Top Left     V1
	Add2DPoint(ex + xoffset, ey - yoffset, color);
	Add2DTexVert(1.0f, 0.0f, color);
	//Bottom Left  V2
	Add2DPoint(ex + xoffset, ey + yoffset, color);
	Add2DTexVert(1.0f, 1.0f, color);
	//Bottom Left  V2
	Add2DPoint(ex + xoffset, ey + yoffset, color);
	Add2DTexVert(1.0f, 1.0f, color);
	//Bottom Right V3
	Add2DPoint(ex - xoffset, ey + yoffset, color);
	Add2DTexVert(0.0f, 1.0f, color);
	//Top Right    V0
	Add2DPoint(ex - xoffset, ey - yoffset, color);
	Add2DTexVert(0.0f, 0.0f, color);
	//Bottom Right V3
	Add2DPoint(ex - xoffset, ey + yoffset, color);
	Add2DTexVert(0.0f, 1.0f, color);
}


void VA::Add2DLine(float x0, float y0, float x1, float y1, rgb_t color)
{
	vertices.push_back(Vertex2D(x0, y0, color));
	vertices.push_back(Vertex2D(x1, y1, color));

}


void VA::Add2DTexQuadVert(float x, float y, float tx, float ty, rgb_t color)
{
	//Vertex2D svertex;
	//TexVertex2D tvertex;
	
	Add2DPoint(x, y, color);
	Add2DTexVert(tx, ty, color);

}


void VA::Clear()
{
	if (this->points) { this->vertices.clear(); }
	if (this->texverts) { this->texvertices.clear(); }

	this->texverts = 0;
	this->points = 0;
}


void VA::SetScale(float newscale)
{
	this->scale = newscale;
}


void VA::Render()
{
	// wrlog("Draw Length %d",points);
	// wrlog("VA_TYPE %d",type);
	// wrlog("USE TEXTURES %x\n",use_textures);



	if (points > 0)
	{

		//Draw Text Array, with 2D, two coordinates per Vertex2D.
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(2, GL_FLOAT, sizeof(Vertex2D), &vertices[0].x);

		if (use_textures)
		{
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, current_tex);
			glBlendFunc(GL_ONE, GL_ONE);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex2D), &texvertices[0].x);
		}
		glEnableClientState(GL_COLOR_ARRAY);
		glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex2D), &vertices[0].colors);

		//glDrawArrays(GL_POINTS ,0, vertices.size());// 4 Coordinates for a Quad. Could use DrawElements here instead GL 3.X+ GL_TRIANGLE_STRIP? 

		glDrawArrays(GL_TRIANGLES, 0, points);

		//Finished Drawing, disable client states.
		glDisableClientState(GL_COLOR_ARRAY);
		if (use_textures)
		{
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			glDisable(GL_TEXTURE_2D);
		}
		glDisableClientState(GL_VERTEX_ARRAY);

	}



}