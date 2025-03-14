// ambientlight.cpp : Defines the entry point for the application.
//

#include "ambientlight.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dcomp.lib")

AmbientLight::AmbientLight(const AppSettings& settings)
{
	m_gameWidth = settings.gameWidth;
	m_gameHeight = settings.gameHeight;
	m_windowWidth = 0;
	m_windowHeight = 0;
	m_hwnd = nullptr;
	m_mirror = settings.mirrored;
	m_blurPasses = settings.blurPasses;
	m_blurSize = settings.blurDownscale;
	m_updateInterval = settings.updateInterval;
}

AmbientLight::~AmbientLight()
{
}

HRESULT AmbientLight::Initialize(HWND hwnd)
{
	m_hwnd = hwnd;
	// create device
	D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
	D3D_FEATURE_LEVEL featureLevel;
	UINT creationFlags = 0;
#if defined(_DEBUG)
	// If the project is in a debug build, enable the debug layer.
	creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, creationFlags, featureLevels, 1, D3D11_SDK_VERSION, &m_device, &featureLevel, &m_context);
	RETURN_IF_FAILED(hr);

	ComPtr<IDXGIDevice> dxgiDevice;
	hr = m_device.As(&dxgiDevice);
	RETURN_IF_FAILED(hr);

	ComPtr<IDXGIFactory2> dxgiFactory2;
	hr = CreateDXGIFactory2(0, __uuidof(IDXGIFactory2), &dxgiFactory2);

	RECT windowRect = { 0 };
	GetWindowRect(hwnd, &windowRect);
	m_windowWidth = RECT_WIDTH(windowRect);
	m_windowHeight = RECT_HEIGHT(windowRect);

	// if missing config, setting default game size to 16:9
	if (m_gameWidth == 0 || m_gameHeight == 0)
	{
		m_gameHeight = m_windowHeight;
		m_gameWidth = m_gameHeight * 16 / 9;
	}

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

	CreateOffscreen(DXGI_FORMAT_B8G8R8A8_UNORM);

	hr = m_capture.Initialize(m_device);
	m_copy.Initialize(m_device);

	m_blurPre.Initialize(m_device, m_gameWidth, m_gameHeight);
	m_blurDownscale.Initialize(m_device, m_blurSize, m_blurSize);
	
	return 0;
}


HRESULT AmbientLight::CreateOffscreen(DXGI_FORMAT format)
{
	HRESULT hr = S_OK;
	m_gameTexture.RecreateTexture(m_device.Get(), format, m_gameWidth, m_gameHeight);
	m_gameTexture1.RecreateTexture(m_device.Get(), format, m_gameWidth, m_gameHeight);

	float aspect = (float)m_gameWidth / (float)m_gameHeight;

	UINT down_width = m_blurSize;
	UINT down_height = m_blurSize;

	m_offscreen1.RecreateTexture(m_device.Get(), format, down_width, down_height);
	m_offscreen2.RecreateTexture(m_device.Get(), format, down_width, down_height);

	m_offscreen3.RecreateTexture(m_device.Get(), format, m_windowWidth, m_windowHeight);
	return hr;
}

void AmbientLight::Render()
{
	m_capture.Capture();

	RenderEffects();
	Present();
}

void AmbientLight::RenderEffects()
{
	ComPtr<ID3D11Texture2D> desktopTexture = m_capture.GetDesktopTexture();
	if (desktopTexture)
	{
		ComPtr<IDXGISurface> surface;
		desktopTexture.As(&surface);
		DXGI_SURFACE_DESC desc;
		surface->GetDesc(&desc);

		UINT black_bar_width = (desc.Width - m_gameWidth) / 2;
		UINT black_bar_height = (desc.Height - m_gameHeight) / 2;

		D3D11_BOX game_box = {};
		game_box.left = black_bar_width;
		game_box.top = black_bar_height;
		game_box.right = desc.Width - black_bar_width;
		game_box.bottom = desc.Height - black_bar_height;
		game_box.front = 0;
		game_box.back = 1;

		m_context->CopySubresourceRegion(m_gameTexture.GetTexture(), 0, 0, 0, 0, desktopTexture.Get(), 0, &game_box);

		for (UINT i = 0; i < m_blurPasses; i++)
		{
			m_blurPre.Apply(m_gameTexture1, m_gameTexture, Blur::BlurDirection::BlurHorizontal);
			m_blurPre.Apply(m_gameTexture, m_gameTexture1, Blur::BlurDirection::BlurVertical);
		}

		m_copy.Apply(m_offscreen1, m_gameTexture);

		for (UINT i = 0; i < m_blurPasses; i++)
		{
			m_blurDownscale.Apply(m_offscreen2, m_offscreen1, Blur::BlurDirection::BlurHorizontal);
			m_blurDownscale.Apply(m_offscreen1, m_offscreen2, Blur::BlurDirection::BlurVertical);
		}

		m_copy.Apply(m_offscreen3, m_offscreen1, m_mirror);
	}
}

void AmbientLight::Present()
{
	UINT black_bar_width = (m_windowWidth - m_gameWidth) / 2;
	UINT black_bar_height = (m_windowHeight - m_gameHeight) / 2;

	D3D11_BOX dst_left = {};
	dst_left.left = 0;
	dst_left.right = black_bar_width;
	dst_left.top = 0;
	dst_left.bottom = m_windowHeight;
	dst_left.front = 0;
	dst_left.back = 1;

	D3D11_BOX dst_right = {};
	dst_right.left = m_windowWidth - black_bar_width;
	dst_right.right = m_windowWidth;
	dst_right.top = 0;
	dst_right.bottom = m_windowHeight;
	dst_right.front = 0;
	dst_right.back = 1;

	ComPtr<ID3D11Texture2D> backBuffer;
	m_swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuffer);

	m_context->CopySubresourceRegion(backBuffer.Get(), 0, dst_left.left, dst_left.top, 0, m_offscreen3.GetTexture(), 0, m_mirror? &dst_right : &dst_left);
	m_context->CopySubresourceRegion(backBuffer.Get(), 0, dst_right.left, dst_right.top, 0, m_offscreen3.GetTexture(), 0, m_mirror ? &dst_left : &dst_right);

	m_swapchain->Present(m_updateInterval, 0);
}

