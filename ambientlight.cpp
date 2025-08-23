// ambientlight.cpp : Defines the entry point for the application.
//

#include "ambientlight.h"
#include "ui.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx11.h"

#include <algorithm>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "Winmm.lib")

#define IS_BOX_EMPTY(box) ((box).left >= (box).right || (box).top >= (box).bottom)

AmbientLight::AmbientLight()
	: m_gameWidth(0),
	m_gameHeight(0),
	m_windowWidth(0),
	m_windowHeight(0),
	m_effectWidth(0),
	m_effectHeight(0),
	m_effectZoom(0),
	m_blurSize(0),
	m_blurPasses(0),
	m_blurSamples(5),
	m_mirror(false),
	m_stretched(false),
	m_frameRate(60),
	m_hwnd(nullptr),
	m_lastPresentTime(0),
	m_perfFreq(0),
	m_showConfigWindow(false),
	m_clearConfigWIndow(false),
	m_topbottom(false),
	m_vignetteEnabled(0),
	m_vignetteIntesity(0.0f),
	m_vignetteRadius(0.0f),
	m_vignetteSmoothness(0.0f)
{
	m_dirtyRects[0] = { 0, 0, 0, 0 };
	m_dirtyRects[1] = { 0, 0, 0, 0 };
}

AmbientLight::~AmbientLight()
{
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT AmbientLight::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui::GetCurrentContext() == nullptr)
		return 0;

	switch (message)
	{
	case WM_KEYDOWN:
	{
		int pressed = (int)wParam;
		if (pressed == VK_ESCAPE)
		{
			ShowConfigWindow(!m_showConfigWindow);
			return 0;
		}
	}
	}

	return ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam);
}

void AmbientLight::UpdateSettings()
{
	ValidateSettings();

	if (m_hwnd)
	{
		m_blurPre.Initialize(m_device, m_deferred, m_gameWidth, m_gameHeight, m_blurSamples);
		m_blurDownscale.Initialize(m_device, m_deferred, m_blurSize, m_blurSize, m_blurSamples);

		float windowAspect = (float)m_windowWidth / (float)m_windowHeight;
		m_vignette.Initialize(m_device, m_deferred, m_vignetteIntesity, m_vignetteRadius, m_vignetteSmoothness, windowAspect);

		CreateOffscreen(DXGI_FORMAT_B8G8R8A8_UNORM);

		m_detection.Initialize(m_device, m_immediate, m_windowWidth, m_windowHeight, m_autoDetectionBrightnessThreshold, m_autoDetectionBlackRatio);
	}
}

void AmbientLight::ValidateSettings()
{
	// Validate game width and height
	UINT width = m_settings.gameWidth;
	UINT height = m_settings.gameHeight;

	m_useAutoDetect = m_settings.useAutoDetection;
	if (m_useAutoDetect)
	{
		width = m_autoWidth;
		height = m_autoHeight;
	}

	// if missing config, setting default game size to 16:9
	if (width == 0 || height == 0)
	{
		width = 16;
		height = 9;
	}

	// use the width/height as aspect ratio, and calculate the game size base on the desktop size
	m_gameHeight = m_windowHeight;
	m_gameWidth = (UINT)std::round((float)m_gameHeight * (float)width / (float)height);

	if (m_gameWidth > m_windowWidth)
	{
		m_gameWidth = m_windowWidth;
		m_gameHeight = (UINT)std::round((float)m_gameWidth * (float)height / (float)width);
	}

	float gameAspect = (float)m_gameWidth / (float)m_gameHeight;
	float windowAspect = (float)m_windowWidth / (float)m_windowHeight;
	if (gameAspect > windowAspect)
	{
		m_topbottom = true;
		m_effectWidth = m_windowWidth;
		m_effectHeight = (m_windowHeight - m_gameHeight) / 2;
	}
	else
	{
		m_topbottom = false;
		m_effectWidth = (m_windowWidth - m_gameWidth) / 2;
		m_effectHeight = m_windowHeight;
	}

	// Validate blur settings
	m_settings.blurPasses = std::clamp(m_settings.blurPasses, 0u, 128u);
	m_settings.blurDownscale = std::clamp(m_settings.blurDownscale, 16u, 1024u);
	m_settings.blurSamples = std::clamp(m_settings.blurSamples / 2 * 2 + 1, 1u, 63u);

	// Validate vignette settings
	m_settings.vignetteIntensity = std::clamp(m_settings.vignetteIntensity, 0.0f, 1.0f);
	m_settings.vignetteRadius = std::clamp(m_settings.vignetteRadius, 0.0f, 1.0f);
	m_settings.vignetteSmoothness = std::clamp(m_settings.vignetteSmoothness, 0.0f, 1.0f);

	// Validate frame rate
	m_settings.frameRate = std::clamp(m_settings.frameRate, 10u, 1000u);

	// Validate zoom
	m_settings.zoom = std::clamp(m_settings.zoom, 0u, 16u);

	m_mirror = m_settings.mirrored;
	m_stretched = m_settings.stretched;
	m_blurPasses = m_settings.blurPasses;
	m_blurSize = m_settings.blurDownscale;
	m_blurSamples = m_settings.blurSamples;
	m_frameRate = m_settings.frameRate;
	m_effectZoom = m_settings.zoom * 16;
	m_vignetteEnabled = m_settings.vignetteEnabled;
	m_vignetteIntesity = m_settings.vignetteIntensity;
	m_vignetteRadius = m_settings.vignetteRadius;
	m_vignetteSmoothness = m_settings.vignetteSmoothness;

	m_autoDetectionBrightnessThreshold = m_settings.autoDetectionBrightnessThreshold;
	m_autoDetectionBlackRatio = m_settings.autoDetectionBlackRatio;
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
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
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

	m_fullscreenQuad.Initialize(m_device, m_deferred);
	m_copy.Initialize(m_device, m_deferred.Get());

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(m_device.Get(), m_deferred.Get());

	ShowConfigWindow(true);

	UpdateSettings();

	return 0;
}

HRESULT AmbientLight::CreateOffscreen(DXGI_FORMAT format)
{
	HRESULT hr = S_OK;
	m_gameTexture.RecreateTexture(m_device.Get(), format, m_gameWidth, m_gameHeight);

	float aspect = (float)m_gameWidth / (float)m_gameHeight;

	UINT down_width = m_blurSize;
	UINT down_height = (UINT)((float)m_blurSize / aspect);

	m_offscreen1.RecreateTexture(m_device.Get(), format, down_width, down_height);

	UINT width2 = m_stretched ? m_windowWidth : m_gameWidth;
	UINT height2 = m_stretched ? m_windowHeight : m_gameHeight;
	m_offscreen2.RecreateTexture(m_device.Get(), format, width2 + m_effectZoom * 2, height2 + m_effectZoom * 2);

	m_offscreen3.RecreateTexture(m_device.Get(), format, m_windowWidth, m_windowHeight);
	return hr;
}

void AmbientLight::Render()
{
	m_capture.Capture();
	ComPtr<ID3D11Texture2D> desktopTexture = m_capture.GetDesktopTexture();
	if (desktopTexture)
	{
		RenderEffects();
		RenderConfig();
		RenderBackBuffer();
		Present();
		Detect();
	}

	bool changed = ReadSettings(m_settings);
	if (changed)
		UpdateSettings();
}

void AmbientLight::RenderEffects()
{
	ComPtr<ID3D11Texture2D> desktopTexture = m_capture.GetDesktopTexture();

	ComPtr<IDXGISurface> surface;
	desktopTexture.As(&surface);
	DXGI_SURFACE_DESC desc = {};
	surface->GetDesc(&desc);

	UINT crop_width = (UINT)std::round((float)(desc.Width - m_gameWidth) / 2);
	UINT crop_height = (UINT)std::round((float)(desc.Height - m_gameHeight) / 2);

	D3D11_BOX game_box = {};
	game_box.left = crop_width;
	game_box.top = crop_height;
	game_box.right = desc.Width - crop_width;
	game_box.bottom = desc.Height - crop_height;
	game_box.front = 0;
	game_box.back = 1;

	if (IS_BOX_EMPTY(game_box))
		return;

	m_deferred->CopySubresourceRegion(m_gameTexture.GetTexture(), 0, 0, 0, 0, desktopTexture.Get(), 0, &game_box);

	m_fullscreenQuad.Render();

	m_blurPre.Render(m_gameTexture, m_blurPasses);

	m_copy.Render(m_offscreen1, m_gameTexture);

	m_blurDownscale.Render(m_offscreen1, m_blurPasses);

	Copy::Flip flip = Copy::FlipNone;
	if (m_mirror)
	{
		flip = m_topbottom ? Copy::FlipVertical : Copy::FlipHorizontal;
	}
	m_copy.Render(m_offscreen2, m_offscreen1, flip);

	ID3D11RenderTargetView* rtv = m_offscreen3.GetRTV();
	float color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_deferred->ClearRenderTargetView(rtv, color);

	D3D11_TEXTURE2D_DESC desc2 = {};
	m_offscreen2.GetTexture()->GetDesc(&desc2);

	D3D11_BOX box0 = {};
	D3D11_BOX box1 = {};
	if (!m_topbottom)
	{
		box0.left = m_effectZoom;
		box0.right = box0.left + m_effectWidth;
		box0.top = m_effectZoom;
		box0.bottom = box0.top + m_effectHeight;
		box0.front = 0;
		box0.back = 1;

		box1.left = desc2.Width - m_effectZoom - m_effectWidth;
		box1.right = desc2.Width - m_effectZoom;
		box1.top = m_effectZoom;
		box1.bottom = box1.top + m_effectHeight;
		box1.front = 0;
		box1.back = 1;

		if (IS_BOX_EMPTY(box0) || IS_BOX_EMPTY(box1))
			return;

		m_dirtyRects[0] = { 0, 0, (LONG)m_effectWidth, (LONG)m_effectHeight };
		m_deferred->CopySubresourceRegion(
			m_offscreen3.GetTexture(), 0,
			0, 0, 0,
			m_offscreen2.GetTexture(), 0,
			m_mirror ? &box1 : &box0);

		m_dirtyRects[1] = { (LONG)(m_effectWidth + m_gameWidth), 0, (LONG)(m_effectWidth * 2 + m_gameWidth), (LONG)m_effectHeight };
		m_deferred->CopySubresourceRegion(
			m_offscreen3.GetTexture(), 0,
			m_effectWidth + m_gameWidth, 0, 0,
			m_offscreen2.GetTexture(), 0,
			m_mirror ? &box0 : &box1);
	}
	else
	{
		box0.left = m_effectZoom;
		box0.right = box0.left + m_effectWidth;
		box0.top = m_effectZoom;
		box0.bottom = box0.top + m_effectHeight;
		box0.front = 0;
		box0.back = 1;

		box1.left = m_effectZoom;
		box1.right = box1.left + m_effectWidth;
		box1.top = desc2.Height - m_effectZoom - m_effectHeight;
		box1.bottom = desc2.Height - m_effectZoom;
		box1.front = 0;
		box1.back = 1;

		if (IS_BOX_EMPTY(box0) || IS_BOX_EMPTY(box1))
			return;

		m_dirtyRects[0] = { 0, 0, (LONG)m_effectWidth, (LONG)m_effectHeight };
		m_deferred->CopySubresourceRegion(
			m_offscreen3.GetTexture(), 0,
			0, 0, 0,
			m_offscreen2.GetTexture(), 0,
			m_mirror ? &box1 : &box0);

		m_dirtyRects[1] = { 0, (LONG)(m_effectHeight + m_gameHeight), (LONG)m_effectWidth, (LONG)(m_effectHeight * 2 + m_gameHeight) };
		m_deferred->CopySubresourceRegion(
			m_offscreen3.GetTexture(), 0,
			0, m_effectHeight + m_gameHeight, 0,
			m_offscreen2.GetTexture(), 0,
			m_mirror ? &box0 : &box1);
	}
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

	if (m_vignetteEnabled)
		m_vignette.Render(backview, m_offscreen3);
	else
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

	if (m_showConfigWindow || m_clearConfigWIndow)
	{
		m_clearConfigWIndow = false;
		m_swapchain->Present(1, 0);
	}
	else
	{
		DXGI_PRESENT_PARAMETERS param = {};
		param.DirtyRectsCount = 2;
		param.pDirtyRects = m_dirtyRects;
		param.pScrollOffset = nullptr;
		param.pScrollRect = nullptr;

		m_swapchain->Present1(1, 0, &param);
	}

	QueryPerformanceCounter((LARGE_INTEGER*)&now);
	m_lastPresentTime = now;
}

void AmbientLight::Detect()
{
	if (m_useAutoDetect)
	{
		static ULONGLONG lastDetection = 0;
		ULONGLONG now = timeGetTime();
		if (now - lastDetection > 500)
		{
			ComPtr<ID3D11Texture2D> desktopTexture = m_capture.GetDesktopTexture();
			if (!desktopTexture)
				return;

			lastDetection = now;
			TextureView desktopTextureView;
			desktopTextureView.CreateViews(m_device.Get(), desktopTexture.Get(), false, true);
			m_detection.Detect(desktopTextureView);

			int detectedGameWidth = m_detection.GetWidth();
			int detectedGameHeight = m_detection.GetHeight();

			//char* dbg = (char*)malloc(256);
			//sprintf_s(dbg, 256, "Detected game size: %d x %d\n", detectedGameWidth, detectedGameHeight);
			//OutputDebugStringA(dbg);

			bool updateSettings = false;
			if (m_autoWidth != detectedGameWidth || m_autoHeight != detectedGameHeight)
			{
				m_autoWidth = detectedGameWidth;
				m_autoHeight = detectedGameHeight;
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
		m_clearConfigWIndow = true;
		m_showConfigWindow = show;
		DWORD dwExStyle = GetWindowLong(m_hwnd, GWL_EXSTYLE);

		dwExStyle = show ? dwExStyle & ~WS_EX_TRANSPARENT : dwExStyle | WS_EX_TRANSPARENT;
		SetWindowLong(m_hwnd, GWL_EXSTYLE, dwExStyle);
	}
}
