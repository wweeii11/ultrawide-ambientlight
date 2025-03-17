#include "present.h"

PresentWindow::PresentWindow()
{
}

PresentWindow::~PresentWindow()
{
}

void PresentWindow::Create(HINSTANCE hInstance, AmbientLight* render)
{
	// Create a window class
	WNDCLASSEX wc = { 0 };
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = L"ambientlight";
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	RegisterClassEx(&wc);


	// use direct composition
	DWORD dwExStyle = WS_EX_NOREDIRECTIONBITMAP | WS_EX_LAYERED | WS_EX_TOPMOST;

	// frameless
	DWORD dwStyle = WS_POPUP;

	// Create a window with the screen size
	RECT desktopRect;
	GetWindowRect(GetDesktopWindow(), &desktopRect);
	HWND hwnd = CreateWindowEx(dwExStyle,
		L"ambientlight",
		L"ambientlight",
		dwStyle,
		0, 0, desktopRect.right, desktopRect.bottom,
		0,
		0,
		hInstance,
		render
	);

	SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);

	// do not capture
	SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);

	m_hwnd = hwnd;

	return;
}

void PresentWindow::Run()
{
	// Show the window
	ShowWindow(m_hwnd, SW_SHOWNORMAL);

	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}

LRESULT CALLBACK PresentWindow::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	AmbientLight* pRender = reinterpret_cast<AmbientLight*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

	if (pRender && pRender->WndProc(hwnd, message, wParam, lParam))
		return true;

	switch (message)
	{
	case WM_CREATE:
	{
		// Save the DXSample* passed in to CreateWindow.
		LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));

		pRender = reinterpret_cast<AmbientLight*>(pCreateStruct->lpCreateParams);
		if (pRender)
		{
			pRender->Initialize(hwnd);
		}
	}
	return 0;

	case WM_PAINT:
	{
		if (pRender)
		{
			pRender->Render();
		}
	}
	return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}


	return DefWindowProc(hwnd, message, wParam, lParam);
}

