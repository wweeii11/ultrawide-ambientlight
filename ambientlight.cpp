// ambientlight.cpp : Defines the entry point for the application.
//

#include "ambientlight.h"
#include "windows.h"

using namespace std;

// Create win32 window
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// Create a window class
	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = DefWindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = L"ambientlight";
	RegisterClass(&wc);
	// Create a window
	HWND hwnd = CreateWindow(L"ambientlight", L"Ambient Light", WS_OVERLAPPEDWINDOW, 0, 0, 800, 600, 0, 0, hInstance, 0);
	// Show the window
	ShowWindow(hwnd, nCmdShow);
	// Message loop
	MSG msg = { 0 };
	while (GetMessage(&msg, 0, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}