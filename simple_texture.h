
#pragma once

#ifndef SIMPLE_TEX_H
#define SIMPLE_TEX_H


#include "glew.h"
#include <string>
#include <vector>
#include <map>

class TEX
{
public:

	GLuint texid;
	int width;
	int height;
	int bpp;
	std::string name;

	TEX() : texid(0), width(0), height(0), bpp(0), name("NNY") { }
};


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TEX *LoadPNG(std::string const &filename, std::string const &archname = std::string(), int numcomponents = 0, int filter = 0);
void UseTexture(GLuint *texture, GLboolean linear, GLboolean mipmapping);
void SetBlendMode(int mode);
void FreeTex(TEX *fonttex);
void SnapShot();

#endif