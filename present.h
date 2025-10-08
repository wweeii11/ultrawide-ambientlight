#pragma once

#define UNICODE

#include "common.h"
#include "ambientlight.h"


class PresentWindow
{
public:
    PresentWindow();
    ~PresentWindow();
    void Create(HINSTANCE hInstance, AmbientLight* render);
    void Run();

    HWND GetHwnd() { return m_hwnd; }

    static void FindAndShow();

protected:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
private:
    HWND m_hwnd;
};
