
// Includes

#include <windows.h>
#include "glew.h"
#include "wglew.h"
#include "log.h"
#include "winfont.h"
#include "rawinput.h"
#include "fileio.h"
#include "ini.h"
#include "mixer.h"
#include "astdx.h"

//Globals
HWND hWnd;
HDC hDC;
int SCREEN_W = 1024;
int SCREEN_H = 768;



// Function Declarations
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void EnableOpenGL(HWND hWnd, HDC * hDC, HGLRC * hRC);
void DisableOpenGL(HWND hWnd, HDC hDC, HGLRC hRC);

int KeyCheck(int keynum)
{
	int i;
	static int hasrun = 0;
	static int keys[256];
	//Init
	if (hasrun == 0) { for (i = 0; i < 256; i++) { keys[i] = 0; }	hasrun = 1; }

	if (!keys[keynum] && key[keynum]) //Return True if not in que
	{
		keys[keynum] = 1;	return 1;
	}
	else if (keys[keynum] && !key[keynum]) //Return False if in que
		keys[keynum] = 0;
	return 0;
}

//========================================================================
// Popup a Windows Error Message, Allegro Style
//========================================================================
void allegro_message(const char *title, const char *message)
{
	MessageBoxA(NULL, message, title, MB_ICONEXCLAMATION | MB_OK);
}


void ViewOrtho(int width, int height)
{
	glViewport(0, 0, width, height);             // Set Up An Ortho View	 
	glMatrixMode(GL_PROJECTION);			  // Select Projection
	glLoadIdentity();						  // Reset The Matrix
	glOrtho(0, width, 0, height, -1, 1);	  // Select Ortho 2D Mode DirectX style(640x480)
	glMatrixMode(GL_MODELVIEW);				  // Select Modelview Matrix
	glLoadIdentity();						  // Reset The Matrix
}


//========================================================================
// Return the Window Handle
//========================================================================
HWND win_get_window()
{
	return hWnd;
}

void swap_buffers()
{
	SwapBuffers(hDC);
}

// WinMain

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int iCmdShow)
{
	WNDCLASS wc;
	
	HGLRC hRC;
	MSG msg;
	BOOL quit = FALSE;
	DWORD dwStyle;
	// register window class
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "GLSample";
	RegisterClass(&wc);

	dwStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;//WS_CAPTION | WS_POPUPWINDOW | WS_VISIBLE
	
	//Adjust the window to account for the frame 
	RECT wr = { 0, 0, SCREEN_W, SCREEN_H };
	AdjustWindowRect(&wr, dwStyle, FALSE);

		// create main window
	hWnd = CreateWindow("GLSample", "OpenGL Sample", dwStyle, 0, 0, wr.right - wr.left, wr.bottom - wr.top,  NULL, NULL, hInstance, NULL);

	
	///////////////// Initialize everything here //////////////////////////////

	LogOpen("testlog.txt");
	wrlog("Opening Log");

	// enable OpenGL for the window
	EnableOpenGL(hWnd, &hDC, &hRC);

	GLenum err = glewInit();
	if (GLEW_OK != err)
	{
		/* Problem: glewInit failed, something is seriously wrong. */
		wrlog("Error: %s\n", glewGetErrorString(err));
	}
	wrlog("Status: Using GLEW %s\n", glewGetString(GLEW_VERSION));

	wglSwapIntervalEXT(1);
	Font_Init(28);
	HRESULT i = RawInput_Initialize(hWnd);
	//dsound_init(hWnd);
	
	
	ViewOrtho(SCREEN_W, SCREEN_H);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	astdelux_init();

	/////////////////// END INITIALIZATION ////////////////////////////////////
	int d = 0;
	
	// program main loop
	while (!quit)
	{
		// check for messages
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{

			// handle or dispatch messages
			if (msg.message == WM_QUIT)
			{
				quit = TRUE;
			}
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

		}
		else
		{
			astdelux_run();

			

		}
	}

	//End Program
	astdelux_end();
	// shutdown OpenGL
	DisableOpenGL(hWnd, hDC, hRC);
	//Stop the Audio Subsystem and release all loaded samples and streams
	mixer_end();
	//End our font
	KillFont();
	//Shutdown logging
	wrlog("Closing Log");
	LogClose();
	

	// destroy the window explicitly
	DestroyWindow(hWnd);

	return msg.wParam;

}

// Window Procedure

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

	switch (message)
	{

	case WM_CREATE:
		return 0;

	case WM_CLOSE:
		PostQuitMessage(0);
		return 0;

	case WM_INPUT: {return RawInput_ProcessInput(hWnd, wParam, lParam); return 0; }

	case WM_DESTROY:
		return 0;


	case WM_SYSCOMMAND:
	{
		switch (wParam & 0xfff0)
		{
		case SC_SCREENSAVE:
		case SC_MONITORPOWER:
		{
			return 0;
		}
		/*
		case SC_CLOSE:
		{
			//I can add a close hook here to trap close button
			quit = 1;
			PostQuitMessage(0);
			break;
		}
		*/
		// User trying to access application menu using ALT?
		case SC_KEYMENU:
			return 0;
		}
		DefWindowProc(hWnd, message, wParam, lParam);
	}


	case WM_KEYDOWN:
		switch (wParam)
		{

		case VK_ESCAPE:
			PostQuitMessage(0);
			return 0;

		}
		return 0;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);

	}

}

// Enable OpenGL

void EnableOpenGL(HWND hWnd, HDC * hDC, HGLRC * hRC)
{
	PIXELFORMATDESCRIPTOR pfd;
	int format;

	// get the device context (DC)
	*hDC = GetDC(hWnd);

	// set the pixel format for the DC
	ZeroMemory(&pfd, sizeof(pfd));
	pfd.nSize = sizeof(pfd);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;
	pfd.cDepthBits = 16;
	pfd.iLayerType = PFD_MAIN_PLANE;
	format = ChoosePixelFormat(*hDC, &pfd);
	SetPixelFormat(*hDC, format, &pfd);

	// create and enable the render context (RC)
	*hRC = wglCreateContext(*hDC);
	wglMakeCurrent(*hDC, *hRC);

}

// Disable OpenGL

void DisableOpenGL(HWND hWnd, HDC hDC, HGLRC hRC)
{
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(hRC);
	ReleaseDC(hWnd, hDC);
}
