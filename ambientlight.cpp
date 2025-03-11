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
	DWORD dwExStyle = WS_EX_NOREDIRECTIONBITMAP;
	DWORD dwStyle = WS_POPUP;
	HWND hwnd = CreateWindowEx(dwExStyle, L"ambientlight", L"Ambient Light", dwStyle, 0, 0, 800, 600, 0, 0, hInstance, 0);

	SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);
	SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);

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
