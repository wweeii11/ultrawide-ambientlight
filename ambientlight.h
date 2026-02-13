#pragma once

#include "common.h"
#include "capture.h"
#include "dcomp.h"
#include "shaders/copy.h"
#include "shaders/blur.h"
#include "shaders/fullscreenquad.h"
#include "shaders/vignette.h"
#include "shaders/detect.h"

class AmbientLight
{
public:
    AmbientLight();
    ~AmbientLight();

    HRESULT Initialize(HWND hwnd);

    void Render();

    LRESULT WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    AppSettings m_settings;
    void UpdateSettings();
    void ValidateSettings();

    HWND m_hwnd;
    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_immediate;
    ComPtr<ID3D11DeviceContext> m_deferred;
    ComPtr<IDXGISwapChain1> m_swapchain;
    ComPtr<IDCompositionDevice> m_dcompDevice;
    ComPtr<IDCompositionTarget> m_dcompTarget;
    ComPtr<IDCompositionVisual> m_dcompVisual;

    DesktopCapture m_capture;
    Blur m_blurDownscale;
    Blur m_blurPre;
    Copy m_copy;
    Vignette m_vignette;
    Detection m_detection;

    bool m_effectRendered;
    bool m_presented;

    UINT m_gameWidth;
    UINT m_gameHeight;
    
    UINT m_windowWidth;
    UINT m_windowHeight;
    
    UINT m_effectZoom;

    std::vector<BlackBar> m_blackBars;

    RECT m_dirtyRects[2];

    UINT m_frameRate;
    INT64 m_lastPresentTime;
    INT64 m_perfFreq;

    TextureView m_gameTexture;
    TextureView m_offscreen1;
    TextureView m_offscreen2;
    TextureView m_offscreen3;
    //TextureView m_offscreen4;

    HRESULT CreateOffscreen(DXGI_FORMAT format);

    bool ShouldRenderEffect();
    bool RenderEffects();
    void RenderConfig();
    void RenderBackBuffer();
    void ClearEffects();

    void Present();

    void Detect();

    void ShowConfigWindow(bool show);
    bool m_showConfigWindow;
    bool m_clearConfigWindow;
};
