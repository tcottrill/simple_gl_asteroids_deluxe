//-----------------------------------------------------------------------------
// Copyright (c) 2011-2012
//
// Rev 11-2012
// Simple class for caching and rendering line drawing geometry
// Eventually this may go into a geometry shader.
// Currently using old school glDrawArrays because glDrawElements would buy nothing here.
// Not using vectors because the extra complexity is not needed here.
// Points can easily be replaced by your favorite vec2 
// TODO::Add Blendmode default to initialization
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//-----------------------------------------------------------------------------

#ifndef EMU_VECTOR_DRAW_H
#define EMU_VECTOR_DRAW_H

#include <vector>
#include "framework.h"
#include "colordefs.h"

#define SHOTSIZE  15

class fpoint
{
public:
	float x;
	float y;
	rgb_t color;

	fpoint(float x, float y, rgb_t color) : x(x), y(y), color(color) {}
	fpoint() : x(0), y(0), color(0) {}
	~fpoint() {};
};


//This too can be replaced 
class txdata
{
public:

	float x, y;
	float tx, ty;
	rgb_t color;

	txdata() : x(0), y(0), tx(0), ty(0), color(0) { }
	txdata(float x, float y, float tx, float ty, rgb_t color) : x(x), y(y), tx(tx), ty(ty), color(color) {}
};

class EmuDraw2D
{
public:

	EmuDraw2D();
	~EmuDraw2D();
	void EmuDraw2D::add_tex(float sx, float sy, float ex, float ey, int intensity, rgb_t col);
	void EmuDraw2D::add_line(float sx, float sy, float ex, float ey, int intensity, rgb_t col);
	void EmuDraw2D::draw_all();
	void EmuDraw2D::set_texture_id(GLuint *id);
	void EmuDraw2D::set_blendmode(GLenum sfactor, GLenum dfactor);
	void EmuDraw2D::blit_any_tex(GLuint *texture, int blending, float alpha, int x, int y, int w, int h);

private:

	std::vector<fpoint> linelist;
	std::vector<txdata> texlist;

	float xoffset;
	float yoffset;
	GLuint *tex;

	void EmuDraw2D::calculate_texture_offsets(int screen_width, int screen_height, int shotsize);
	rgb_t EmuDraw2D::cache_color(int intensity, rgb_t col);
	rgb_t EmuDraw2D::cache_tex_color(int intensity, rgb_t col);
	void EmuDraw2D::cache_texpoint(float ex, float ey, float tx, float ty, int intensity, rgb_t col);
};


#endif