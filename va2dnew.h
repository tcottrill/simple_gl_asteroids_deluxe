//Specialized simple Vertex Array code for testing.

#ifndef HVA
#define HVA

#include <string>
#include <sstream> 
#include <vector>
#include "colordefs.h"
#include "glew.h"
#include "wglew.h"


// Include GLM
#include <c:/source/glm/glm/glm.hpp>
#include <c:/source/glm/glm/gtc/matrix_transform.hpp>

using namespace glm;

#define USE_OPENGL_TRIANGLES 1



#define VA_POINT      0
#define VA_LINE       1
#define VA_QUAD       2
#define VA_TEXQUAD    3

 

class Vertex2D
{
public:
    float x,y;
	vec2 a;
  	rgb_t colors;
	Vertex2D() :  x( 0 ), y( 0 ), colors(0) { }
	Vertex2D(float x, float y, rgb_t colors): x(x), y(y), colors(colors) {} 
};


class VA
{
private:
	int points;
	int texverts;
	float scale;
	int type;
	int use_textures;
	GLuint current_tex;
	std::vector<Vertex2D> vertices;
    std::vector<Vertex2D> texvertices;
	Vertex2D vertex;
	Vertex2D tvertex;
	void Add2DTexVert(float x, float y , rgb_t color );
  
public:
  VA (int);
  ~VA ();
 void VA::SetScale(float newscale);
 void VA::SetActiveTexture(GLuint tex){current_tex=tex;};
 void VA::SetType(int ntype);
 void VA::Add2DPoint(float x, float y , rgb_t color );
 void VA::Add2DLine(float x0, float y0, float x1, float y1, rgb_t color );
 void VA::Add2DQuad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3 , rgb_t color );
 void VA::Add2DScaledQuad(float x, float y , rgb_t color );
 void VA::Add2DTexQuadVert(float x, float y , float tx, float ty, rgb_t color );
 void VA::Add2DCenteredTex(float sx, float sy ,float ex, float ey , int intensity, rgb_t color );
 void VA::Clear();
 void VA::Render();
 
};


#endif