#include "present.h"
#include "resources.h"
#include "shellapi.h"

#define APP_WINDOW_CLASS_NAME L"ambientlightapp"

PresentWindow::PresentWindow()
{
}

PresentWindow::~PresentWindow()
{
}

void PresentWindow::FindAndShow()
{
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL
        {
            wchar_t className[256];
            GetClassName(hwnd, className, 256);
            if (wcscmp(className, APP_WINDOW_CLASS_NAME) == 0)
            {
                ShowWindow(hwnd, SW_SHOWNORMAL);
                SetForegroundWindow(hwnd);
                return FALSE; // stop enumeration
            }
            return TRUE; // continue enumeration
        }, NULL);
}

void PresentWindow::Create(HINSTANCE hInstance, AmbientLight* render)
{
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);

    // Create a window class
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = APP_WINDOW_CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.hIcon = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDB_APPICON), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    RegisterClassEx(&wc);

    // use direct composition
    DWORD dwExStyle = WS_EX_NOREDIRECTIONBITMAP | WS_EX_LAYERED | WS_EX_TOPMOST;

    // frameless
    DWORD dwStyle = WS_POPUP;

    // Create a window with the screen size
    RECT desktopRect;
    GetWindowRect(GetDesktopWindow(), &desktopRect);
    HWND hwnd = CreateWindowEx(dwExStyle,
        APP_WINDOW_CLASS_NAME,
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

    NOTIFYICONDATA nid;
    memset(&nid, 0, sizeof(nid)); // Initialize to zeros
    nid.cbSize = sizeof(nid);
    nid.hWnd = m_hwnd; // Handle to your application's main window
    nid.uID = IDB_APPICON; // Unique ID for your icon (from resource file or a constant)
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; // Flags indicating what information is provided
    nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDB_APPICON)); // Handle to your icon
    wcscpy_s(nid.szTip, L"Ambientlight"); // Tooltip text
    nid.uCallbackMessage = WM_USER_SHELLICON; // Your custom message ID

    Shell_NotifyIcon(NIM_ADD, &nid);

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

    case WM_DISPLAYCHANGE:
    {
        RECT desktopRect;
        GetWindowRect(GetDesktopWindow(), &desktopRect);
        SetWindowPos(hwnd, nullptr, 0, 0, desktopRect.right, desktopRect.bottom, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        pRender->Initialize(hwnd);
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

