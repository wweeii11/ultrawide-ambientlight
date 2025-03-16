// ambientlight.cpp : Defines the entry point for the application.
//

#include "ambientlight.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dcomp.lib")

AmbientLight::AmbientLight()
{
	m_gameWidth = 0;
	m_gameHeight = 0;
	m_windowWidth = 0;
	m_windowHeight = 0;
	m_effectWidth = 0;
	m_effectHeight = 0;
	m_effectZoom = 0;
	m_blurSize = 0;
	m_blurPasses = 0;
	m_mirror = false;
	m_updateInterval = 2;
	m_hwnd = nullptr;
}

AmbientLight::~AmbientLight()
{
}

void AmbientLight::UpdateSettings()
{
	bool changed = ReadSettings(m_settings);

	if (!changed)
		return;

	UINT width = m_settings.gameWidth;
	UINT height = m_settings.gameHeight;
	bool useAspect = m_settings.useAspectRatio;

	m_mirror = m_settings.mirrored;
	m_blurPasses = m_settings.blurPasses;
	m_blurSize = m_settings.blurDownscale;
	m_updateInterval = m_settings.updateInterval;
	m_topbottom = false;
	
	if (m_hwnd)
	{
		// if missing config, setting default game size to 16:9
		if (width == 0 || height == 0)
		{
			useAspect = true;
			width = 16;
			height = 9;
		}

		// use the resolution as is for cropping the game image
		if (!useAspect)
		{
			m_gameWidth = width;
			m_gameHeight = height;
		}
		else
		{
			// use the width/height as aspect ratio, and calculate the game size base on the desktop size
			m_gameHeight = m_windowHeight;
			m_gameWidth = m_gameHeight * width / height;

			if (m_gameWidth > m_windowWidth)
			{
				m_gameWidth = m_windowWidth;
				m_gameHeight = m_gameWidth * height / width;
			}
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

		m_effectZoom = 16;

		m_blurPre.Initialize(m_device, m_gameWidth, m_gameHeight);
		m_blurDownscale.Initialize(m_device, m_blurSize, m_blurSize);

		CreateOffscreen(DXGI_FORMAT_B8G8R8A8_UNORM);
	}
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
	m_copy.Initialize(m_device);

	UpdateSettings();

	return 0;
}

HRESULT AmbientLight::CreateOffscreen(DXGI_FORMAT format)
{
	HRESULT hr = S_OK;
	m_gameTexture.RecreateTexture(m_device.Get(), format, m_gameWidth, m_gameHeight);

	float aspect = (float)m_gameWidth / (float)m_gameHeight;

	UINT down_width = m_blurSize;
	UINT down_height = m_blurSize;

	m_offscreen1.RecreateTexture(m_device.Get(), format, down_width, down_height);

	m_offscreen3.RecreateTexture(m_device.Get(), format, m_windowWidth + m_effectZoom * 2, m_windowHeight + m_effectZoom * 2);
	return hr;
}

void AmbientLight::Render()
{
	UpdateSettings();

	m_capture.Capture();
	ComPtr<ID3D11Texture2D> desktopTexture = m_capture.GetDesktopTexture();
	if (desktopTexture)
	{
		RenderEffects();
		Present();
	}
}

void AmbientLight::RenderEffects()
{
	ComPtr<ID3D11Texture2D> desktopTexture = m_capture.GetDesktopTexture();

	ComPtr<IDXGISurface> surface;
	desktopTexture.As(&surface);
	DXGI_SURFACE_DESC desc;
	surface->GetDesc(&desc);

	UINT crop_width = (desc.Width - m_gameWidth) / 2;
	UINT crop_height = (desc.Height - m_gameHeight) / 2;

	D3D11_BOX game_box = {};
	game_box.left = crop_width;
	game_box.top = crop_height;
	game_box.right = desc.Width - crop_width;
	game_box.bottom = desc.Height - crop_height;
	game_box.front = 0;
	game_box.back = 1;

	m_context->CopySubresourceRegion(m_gameTexture.GetTexture(), 0, 0, 0, 0, desktopTexture.Get(), 0, &game_box);

	m_blurPre.Apply(m_gameTexture, m_blurPasses);

	m_copy.Apply(m_offscreen1, m_gameTexture);

	m_blurDownscale.Apply(m_offscreen1, m_blurPasses);

	Copy::Flip flip = Copy::FlipNone;
	if (m_mirror)
	{
		flip = m_topbottom ? Copy::FlipVertical : Copy::FlipHorizontal;
	}
	m_copy.Apply(m_offscreen3, m_offscreen1, flip);
}

void AmbientLight::Present()
{
	ComPtr<ID3D11Texture2D> backBuffer;
	m_swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuffer);

	ComPtr<ID3D11RenderTargetView> rtv_back;
	m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtv_back.GetAddressOf());
	m_context->OMSetRenderTargets(1, rtv_back.GetAddressOf(), nullptr);
	float color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_context->ClearRenderTargetView(rtv_back.Get(), color);

	if (!m_topbottom)
	{
		D3D11_BOX src_left = {};
		src_left.left = m_effectZoom;
		src_left.right = src_left.left + m_effectWidth;
		src_left.top = m_effectZoom;
		src_left.bottom = src_left.top + m_effectHeight;
		src_left.front = 0;
		src_left.back = 1;

		D3D11_BOX src_right = {};
		src_right.left = src_left.right + m_gameWidth;
		src_right.right = src_right.left + m_effectWidth;
		src_right.top = m_effectZoom;
		src_right.bottom = src_right.top + m_effectHeight;
		src_right.front = 0;
		src_right.back = 1;

		m_context->CopySubresourceRegion(
			backBuffer.Get(), 0,
			0, 0, 0,
			m_offscreen3.GetTexture(), 0,
			m_mirror ? &src_right : &src_left);

		m_context->CopySubresourceRegion(
			backBuffer.Get(), 0,
			m_effectWidth + m_gameWidth, 0, 0,
			m_offscreen3.GetTexture(), 0,
			m_mirror ? &src_left : &src_right);
	}
	else
	{
		D3D11_BOX src_top = {};
		src_top.left = m_effectZoom;
		src_top.right = src_top.left + m_effectWidth;
		src_top.top = m_effectZoom;
		src_top.bottom = src_top.top + m_effectHeight;
		src_top.front = 0;
		src_top.back = 1;

		D3D11_BOX src_bottom = {};
		src_bottom.left = m_effectZoom;
		src_bottom.right = src_bottom.left + m_effectWidth;
		src_bottom.top = src_top.bottom + m_gameHeight;
		src_bottom.bottom = src_bottom.top + m_effectHeight;
		src_bottom.front = 0;
		src_bottom.back = 1;

		m_context->CopySubresourceRegion(
			backBuffer.Get(), 0,
			0, 0, 0,
			m_offscreen3.GetTexture(), 0,
			m_mirror ? &src_bottom : &src_top);

		m_context->CopySubresourceRegion(
			backBuffer.Get(), 0,
			0, m_effectHeight + m_gameHeight, 0,
			m_offscreen3.GetTexture(), 0,
			m_mirror ? &src_top : &src_bottom);
	}
	m_swapchain->Present(m_updateInterval, 0);
}

