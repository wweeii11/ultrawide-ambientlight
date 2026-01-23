// ambientlight.cpp : Defines the entry point for the application.
//

#include "ambientlight.h"
#include "ui.h"

#include <algorithm>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "Winmm.lib")

#define IS_BOX_EMPTY(box) ((box).left >= (box).right || (box).top >= (box).bottom)

D3D11_BOX GetMirroredBox(D3D11_BOX box, UINT width, UINT height)
{
    D3D11_BOX mirrored = box;
    if (RECT_HEIGHT(box) == height)
    {
        // left/right
        mirrored.left = width - box.right;
        mirrored.right = width - box.left;
    }
    else if (RECT_WIDTH(box) == width)
    {
        // top/bottom
        mirrored.top = height - box.bottom;
        mirrored.bottom = height - box.top;
    }
    return mirrored;
}

AmbientLight::AmbientLight()
    : m_effectRendered(false),
    m_presented(false),
    m_gameWidth(0),
    m_gameHeight(0),
    m_windowWidth(0),
    m_windowHeight(0),
    m_effectZoom(0),
    m_frameRate(60),
    m_hwnd(nullptr),
    m_lastPresentTime(0),
    m_perfFreq(0),
    m_showConfigWindow(false),
    m_clearConfigWindow(false)
{
    m_dirtyRects[0] = { 0, 0, 0, 0 };
    m_dirtyRects[1] = { 0, 0, 0, 0 };
}

AmbientLight::~AmbientLight()
{
}

LRESULT AmbientLight::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_TOGGLE_CONFIG_WINDOW:
    {
        bool toggle = (lParam != 0);
        bool show = toggle ? !m_showConfigWindow : (wParam != 0);
        ShowConfigWindow(show);
        if (show)
            SetForegroundWindow(hwnd);
        return 0;
    }
    }

    return UiWndProc(hwnd, message, wParam, lParam);
}

void AmbientLight::UpdateSettings()
{
    ValidateSettings();

    if (m_hwnd)
    {
        m_blurPre.Initialize(m_device,
            m_deferred,
            m_gameWidth,
            m_gameHeight,
            m_settings.blurSamples);
        m_blurDownscale.Initialize(m_device,
            m_deferred,
            m_settings.blurDownscale,
            m_settings.blurDownscale,
            m_settings.blurSamples);

        float windowAspect = (float)m_windowWidth / (float)m_windowHeight;
        m_vignette.Initialize(m_device,
            m_deferred,
            m_settings.vignetteIntensity,
            m_settings.vignetteRadius,
            m_settings.vignetteSmoothness,
            windowAspect);

        CreateOffscreen(DXGI_FORMAT_B8G8R8A8_UNORM);

        m_detection.Initialize(m_device,
            m_immediate,
            m_windowWidth,
            m_windowHeight,
            m_settings.autoDetectionBrightnessThreshold,
            m_settings.autoDetectionBlackRatio,
            m_settings.autoDetectionSymmetricBars,
            m_settings.autoDetectionReservedArea ? m_settings.autoDetectionReservedWidth : 0,
            m_settings.autoDetectionReservedArea ? m_settings.autoDetectionReservedHeight : 0);

        UpdateUI(m_hwnd, m_settings);
    }
}

void AmbientLight::ValidateSettings()
{
    if (m_settings.loaded && m_settings.useAutoDetection)
    {
        m_blackBars = m_detection.GetDetectedBoxes();
    }
    else
    {
        // Validate game width and height
        UINT width = m_settings.gameWidth;
        UINT height = m_settings.gameHeight;

        if (!m_settings.loaded)
        {
            // initial value
            width = m_windowWidth;
            height = m_windowHeight;
        }
        else if (width == 0 || height == 0)
        {
            // if missing config, setting default game size to 16:9
            width = 16;
            height = 9;
        }

        m_blackBars = m_detection.GetFixedBoxes(width, height);
    }


    // use the width/height as aspect ratio, and calculate the game size base on the desktop size
    m_gameWidth = m_windowWidth;
    m_gameHeight = m_windowHeight;
    if (m_blackBars.size() == 2)
    {
        if (m_windowWidth > RECT_WIDTH(m_blackBars[0]))
            m_gameWidth = m_windowWidth - RECT_WIDTH(m_blackBars[0]) - RECT_WIDTH(m_blackBars[1]);
        if (m_windowHeight > RECT_HEIGHT(m_blackBars[0]))
            m_gameHeight = m_windowHeight - RECT_HEIGHT(m_blackBars[0]) - RECT_HEIGHT(m_blackBars[1]);
    }

    // Validate blur settings
    m_settings.blurPasses = std::clamp(m_settings.blurPasses, 0u, 128u);
    m_settings.blurDownscale = std::clamp(m_settings.blurDownscale / 2 * 2, 16u, 1024u);
    m_settings.blurSamples = std::clamp(m_settings.blurSamples / 2 * 2 + 1, 1u, 63u);

    // Validate vignette settings
    m_settings.vignetteIntensity = std::clamp(m_settings.vignetteIntensity, 0.0f, 1.0f);
    m_settings.vignetteRadius = std::clamp(m_settings.vignetteRadius, 0.0f, 1.0f);
    m_settings.vignetteSmoothness = std::clamp(m_settings.vignetteSmoothness, 0.0f, 1.0f);

    // Validate frame rate
    m_settings.frameRate = std::clamp(m_settings.frameRate, 10u, 1000u);
    m_frameRate = m_settings.frameRate;

    // Validate zoom
    m_settings.zoom = std::clamp(m_settings.zoom, 0u, 16u);
    m_effectZoom = m_settings.zoom * 16;
}

HRESULT AmbientLight::Initialize(HWND hwnd)
{
    m_hwnd = hwnd;

    QueryPerformanceFrequency((LARGE_INTEGER*)&m_perfFreq);
    timeBeginPeriod(1);

    // create device
    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL featureLevel;
    UINT creationFlags = 0;
#if defined(_DEBUG)
    // If the project is in a debug build, enable the debug layer.
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, creationFlags, featureLevels, 1, D3D11_SDK_VERSION, &m_device, &featureLevel, &m_immediate);
    RETURN_IF_FAILED(hr);

    m_device->CreateDeferredContext(0, &m_deferred);

    ComPtr<IDXGIDevice> dxgiDevice;
    hr = m_device.As(&dxgiDevice);
    RETURN_IF_FAILED(hr);

    ComPtr<IDXGIFactory2> dxgiFactory2;
    hr = CreateDXGIFactory2(0, __uuidof(IDXGIFactory2), &dxgiFactory2);

    RECT windowRect = { 0 };
    GetWindowRect(hwnd, &windowRect);
    m_windowWidth = RECT_WIDTH(windowRect);
    m_windowHeight = RECT_HEIGHT(windowRect);

    // create swap chain
    DXGI_SWAP_CHAIN_DESC1 scd = {};
    scd.Width = m_windowWidth;
    scd.Height = m_windowHeight;
    scd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_UNORDERED_ACCESS;
    scd.BufferCount = 2;
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    scd.Scaling = DXGI_SCALING_STRETCH;

    hr = dxgiFactory2->CreateSwapChainForComposition(m_device.Get(), &scd, nullptr, &m_swapchain);
    RETURN_IF_FAILED(hr);

    hr = DCompositionCreateDevice(dxgiDevice.Get(), __uuidof(IDCompositionDevice), &m_dcompDevice);
    RETURN_IF_FAILED(hr);

    hr = m_dcompDevice->CreateTargetForHwnd(m_hwnd, TRUE, &m_dcompTarget);
    RETURN_IF_FAILED(hr);

    hr = m_dcompDevice->CreateVisual(&m_dcompVisual);
    RETURN_IF_FAILED(hr);

    hr = m_dcompVisual->SetContent(m_swapchain.Get());
    RETURN_IF_FAILED(hr);

    hr = m_dcompTarget->SetRoot(m_dcompVisual.Get());
    RETURN_IF_FAILED(hr);

    hr = m_dcompDevice->Commit();
    RETURN_IF_FAILED(hr);

    hr = m_capture.Initialize(m_device);

    m_copy.Initialize(m_device, m_deferred.Get());

    m_gameTexture.Clear();
    m_offscreen1.Clear();
    m_offscreen2.Clear();
    m_offscreen3.Clear();

    InitUI(hwnd, m_device.Get(), m_deferred.Get());

    UpdateSettings();

    return 0;
}

HRESULT AmbientLight::CreateOffscreen(DXGI_FORMAT format)
{
    HRESULT hr = S_OK;
    m_gameTexture.RecreateTexture(m_device.Get(), format, m_gameWidth, m_gameHeight);

    float aspect = (float)m_gameWidth / (float)m_gameHeight;

    UINT downscale = m_settings.blurDownscale;
    m_offscreen1.RecreateTexture(m_device.Get(), format,
        downscale,
        downscale);

    UINT width2 = m_settings.stretched ? m_windowWidth : m_gameWidth;
    UINT height2 = m_settings.stretched ? m_windowHeight : m_gameHeight;
    m_offscreen2.RecreateTexture(m_device.Get(), format,
        width2 + m_effectZoom * 2,
        height2 + m_effectZoom * 2);

    m_offscreen3.RecreateTexture(m_device.Get(), format,
        m_windowWidth,
        m_windowHeight);
    return hr;
}

void AmbientLight::Render()
{
    m_capture.ReleaseFrame();

    if (ShouldRenderEffect())
    {
        RenderEffects();
    }
    else
    {
        ClearEffects();
    }

    RenderConfig();
    RenderBackBuffer();
    Present();
    Detect();

    bool changed = ReadSettings(m_settings);
    if (changed)
        UpdateSettings();
}

bool AmbientLight::ShouldRenderEffect()
{
    return m_gameWidth < m_windowWidth || m_gameHeight < m_windowHeight;
}

bool AmbientLight::RenderEffects()
{
    m_capture.Capture();
    ComPtr<ID3D11Texture2D> desktopTexture = m_capture.GetDesktopTexture();
    if (!desktopTexture)
        return false;

    ComPtr<IDXGISurface> surface;
    desktopTexture.As(&surface);
    DXGI_SURFACE_DESC desc = {};
    surface->GetDesc(&desc);

    if (m_blackBars.size() != 2)
        return false;

    D3D11_BOX game_box = {};
    game_box.front = 0;
    game_box.back = 1;
    if (m_gameHeight == m_windowHeight)
    {
        // black bars on left/right
        game_box.left = RECT_WIDTH(m_blackBars[0]);
        game_box.right = m_windowWidth - RECT_WIDTH(m_blackBars[1]);
        game_box.top = 0;
        game_box.bottom = m_windowHeight;
    }
    else if (m_gameWidth == m_windowWidth)
    {
        // black bars on top/bottom
        game_box.left = 0;
        game_box.right = m_windowWidth;
        game_box.top = RECT_HEIGHT(m_blackBars[0]);
        game_box.bottom = m_windowHeight - RECT_HEIGHT(m_blackBars[1]);
    }
    else
    {
        return false;
    }

    if (IS_BOX_EMPTY(game_box))
        return false;

    m_deferred->CopySubresourceRegion(m_gameTexture.GetTexture(), 0, 0, 0, 0, desktopTexture.Get(), 0, &game_box);

    m_blurPre.Render(m_deferred.Get(), m_gameTexture, m_settings.blurPasses);

    m_copy.Render(m_deferred.Get(), m_offscreen1, m_gameTexture);

    m_blurDownscale.Render(m_deferred.Get(), m_offscreen1, m_settings.blurPasses);

    Copy::Flip flip = Copy::FlipNone;
    if (m_settings.mirrored)
    {
        flip = (m_gameWidth == m_windowWidth) ? Copy::FlipVertical : Copy::FlipHorizontal;
    }
    m_copy.Render(m_deferred.Get(), m_offscreen2, m_offscreen1, flip);

    ID3D11RenderTargetView* rtv = m_offscreen3.GetRTV();
    float color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_deferred->ClearRenderTargetView(rtv, color);

    D3D11_TEXTURE2D_DESC desc2 = {};
    m_offscreen2.GetTexture()->GetDesc(&desc2);


    for (int i = 0; i < 2; i++)
    {
        D3D11_BOX src = m_settings.mirrored ? GetMirroredBox(m_blackBars[i], m_windowWidth, m_windowHeight) : m_blackBars[i];
        D3D11_BOX dst = m_blackBars[i];

        if (!m_settings.stretched)
        {
            // adjust src box from windows size to game size
            if (m_gameHeight == m_windowHeight)
            {
                if (m_gameWidth > RECT_WIDTH(src))
                {
                    if (src.left > m_windowWidth / 2)
                    {
                        src.left -= (m_windowWidth - m_gameWidth);
                        src.right -= (m_windowWidth - m_gameWidth);
                    }
                }
                else
                {
                    // if game width is smaller than black bar width, clamp to game width
                    if (src.left > m_windowWidth / 2)
                    {
                        src.left = m_gameWidth / 2;
                        src.right = m_gameWidth;
                    }
                    else
                    {
                        src.left = 0;
                        src.right = m_gameWidth / 2;
                    }
                }
            }
            else if (m_gameWidth == m_windowWidth)
            {
                if (m_gameHeight > RECT_HEIGHT(src))
                {
                    if (src.top > m_windowHeight / 2)
                    {
                        src.top -= (m_windowHeight - m_gameHeight);
                        src.bottom -= (m_windowHeight - m_gameHeight);
                    }
                }
                else
                {
                    if (src.top > m_windowHeight / 2)
                    {
                        src.top = m_gameHeight / 2;
                        src.bottom = m_gameHeight;
                    }
                    else
                    {
                        src.top = 0;
                        src.bottom = m_gameHeight / 2;
                    }
                }
            }
        }

        src.left += m_effectZoom;
        src.right += m_effectZoom;
        src.top += m_effectZoom;
        src.bottom += m_effectZoom;
        if (IS_BOX_EMPTY(src) || IS_BOX_EMPTY(dst))
            continue;

        m_deferred->CopySubresourceRegion(
            m_offscreen3.GetTexture(), 0,
            dst.left, dst.top, 0,
            m_offscreen2.GetTexture(), 0,
            &src);
    }

    m_effectRendered = true;

    return true;
}

void AmbientLight::ClearEffects()
{
    if (m_effectRendered)
    {
        ID3D11RenderTargetView* rtv = m_offscreen3.GetRTV();
        float color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        m_deferred->ClearRenderTargetView(rtv, color);

        // force next present
        m_presented = false;
    }
    m_effectRendered = false;
}

void AmbientLight::RenderConfig()
{
    if (!m_showConfigWindow)
        return;

    bool open = RenderUI(m_settings, m_gameWidth, m_gameHeight);
    ShowConfigWindow(open);
}

void AmbientLight::RenderBackBuffer()
{
    ComPtr<ID3D11Texture2D> backBuffer;
    m_swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuffer);

    TextureView backview;
    backview.CreateViews(m_device.Get(), backBuffer.Get(), true, false);

    ID3D11RenderTargetView* rtv_back = backview.GetRTV();
    float color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_deferred->ClearRenderTargetView(rtv_back, color);

    if (m_effectRendered)
    {
        if (m_settings.vignetteEnabled)
            m_vignette.Render(m_deferred.Get(), m_offscreen3);

        if (m_settings.useAutoDetection && m_settings.autoDetectionLightMask)
            m_detection.RenderMask(m_deferred.Get(), m_offscreen3);
    }

    m_deferred->CopyResource(backview.GetTexture(), m_offscreen3.GetTexture());

    m_deferred->OMSetRenderTargets(1, &rtv_back, nullptr);
    if (m_showConfigWindow)
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    ComPtr<ID3D11CommandList> cmdlist = nullptr;
    HRESULT hr;
    hr = m_deferred->FinishCommandList(FALSE, &cmdlist);

    if (cmdlist)
    {
        m_immediate->ExecuteCommandList(cmdlist.Get(), TRUE);
    }
}

void AmbientLight::Present()
{
    INT64 now = 0;
    double elapsedMs = 0.0f;
    double frameTime = 1000.0 / m_frameRate;

    while (elapsedMs < (frameTime - 1.0))
    {
        QueryPerformanceCounter((LARGE_INTEGER*)&now);

        if (m_lastPresentTime == 0)
            break;

        INT64 elapsed = now - m_lastPresentTime;
        elapsedMs = (double)(elapsed * 1000 / m_perfFreq);

        if (elapsedMs < (frameTime - 2.0))
        {
            Sleep(1);
        }
        else
        {
            Sleep(0);
        }
    }

    if (m_showConfigWindow || m_clearConfigWindow)
    {
        m_clearConfigWindow = false;
        m_swapchain->Present(1, 0);
        m_presented = true;
    }
    else
    {
        if (!m_presented)
        {
            memset(m_dirtyRects, 0, sizeof(m_dirtyRects));
            int numRects = 0;
            for (auto& box : m_blackBars)
            {
                if (IS_BOX_EMPTY(box))
                    continue;
                m_dirtyRects[numRects].left = box.left;
                m_dirtyRects[numRects].top = box.top;
                m_dirtyRects[numRects].right = box.right;
                m_dirtyRects[numRects].bottom = box.bottom;
                numRects++;
            }

            DXGI_PRESENT_PARAMETERS param = {};
            param.DirtyRectsCount = numRects;
            param.pDirtyRects = m_dirtyRects;
            param.pScrollOffset = nullptr;
            param.pScrollRect = nullptr;

            HRESULT hr = m_swapchain->Present1(1, 0, &param);
            if (FAILED(hr))
            {
                // in case of error, try normal present
                hr = m_swapchain->Present(1, 0);
            }
            m_presented = true;
        }
        else if (!m_effectRendered)
        {
            // if effect is not rendered, and already presented, do nothing
        }
        else
        {
            m_presented = false;
        }
    }

    QueryPerformanceCounter((LARGE_INTEGER*)&now);
    m_lastPresentTime = now;
}

void AmbientLight::Detect()
{
    if (m_settings.useAutoDetection)
    {
        static ULONGLONG lastDetection = 0;
        ULONGLONG now = timeGetTime();
        if (now - lastDetection > m_settings.autoDetectionTime)
        {
            m_capture.Capture();
            ComPtr<ID3D11Texture2D> desktopTexture = m_capture.GetDesktopTexture();
            if (!desktopTexture)
                return;

            lastDetection = now;
            TextureView desktopTextureView;
            desktopTextureView.CreateViews(m_device.Get(), desktopTexture.Get(), false, true, false);
            m_detection.Detect(m_immediate.Get(), desktopTextureView);

            std::vector<D3D11_BOX> detected = m_detection.GetDetectedBoxes();

            bool updateSettings = false;
            if (detected.size() == m_blackBars.size())
            {
                for (int i = 0; i < detected.size(); i++)
                {
                    if (detected[i].left != m_blackBars[i].left ||
                        detected[i].top != m_blackBars[i].top ||
                        detected[i].right != m_blackBars[i].right ||
                        detected[i].bottom != m_blackBars[i].bottom)
                    {
                        updateSettings = true;
                        break;
                    }
                }
            }
            else
            {
                updateSettings = true;
            }

            if (updateSettings)
            {
                UpdateSettings();
            }
        }
    }
}

void AmbientLight::ShowConfigWindow(bool show)
{
    if (show != m_showConfigWindow)
    {
        m_clearConfigWindow = true;
        m_showConfigWindow = show;
        DWORD dwExStyle = GetWindowLong(m_hwnd, GWL_EXSTYLE);

        dwExStyle = show ? dwExStyle & ~WS_EX_TRANSPARENT : dwExStyle | WS_EX_TRANSPARENT;
        SetWindowLong(m_hwnd, GWL_EXSTYLE, dwExStyle);
    }
}
