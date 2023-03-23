#pragma once

#include "framework.h"


enum class fboFilter
{
	FB_NEAREST,
	FB_LINEAR,
	FB_MIPMAP
};

class multifbo
{

public:

	GLuint frameBufID = 0;
	multifbo::multifbo(int width, int height, int num_buffers, int num_samples, fboFilter filter);
	multifbo::~multifbo();
	
	void multifbo::Bind(short buff, short num);
	void multifbo::Check();
	void multifbo::Use();
	void multifbo::UnUse();
	void multifbo::SwitchTo();
	void multifbo::BindToShader(short buff, short num);
	int multifbo::checkFboStatus();


private:

	int numSamples = 0;
	int numBuffers = 0;
	int iMinFilter=GL_NEAREST;
	int iMaxFilter=GL_NEAREST;
	GLuint colorTexID0 = 0;
	GLuint colorTexID1 = 0;
	GLuint depthTexID = 0;
	GLuint colorBufID = 0;
	GLuint depthBufID = 0;
	
	GLsizei iWidth = 1024;
	GLsizei iHeight = 768;
};