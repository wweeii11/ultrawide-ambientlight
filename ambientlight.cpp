// ambientlight.cpp : Defines the entry point for the application.
//

#include "ambientlight.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dcomp.lib")

AmbientLight::AmbientLight(UINT gameWidth, UINT gameHeight)
{
	m_gameWidth = gameWidth;
	m_gameHeight = gameHeight;
	m_windowWidth = 0;
	m_windowHeight = 0;
	m_blurSize = 64;
	m_hwnd = nullptr;
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

	//hr = dxgiFactory2->CreateSwapChainForHwnd(m_device.Get(), hwnd, &scd, nullptr, nullptr, &m_swapchain);
	//RETURN_IF_FAILED(hr);

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
	m_blur.Initialize(m_device, m_blurSize, m_blurSize);

	return 0;
}

HRESULT AmbientLight::RecreateTexture(DXGI_FORMAT format, UINT width, UINT height, TextureView& textureview)
{
	HRESULT hr = S_OK;
	if (textureview.GetTexture())
	{
		ID3D11Texture2D* texture = textureview.GetTexture();
		D3D11_TEXTURE2D_DESC desc;
		texture->GetDesc(&desc);

		if (desc.Format != format || desc.Width != width || desc.Height != height)
		{
			textureview.Clear();
		}
	}
	if (!textureview.GetTexture())
	{
		ID3D11Texture2D* texture = nullptr;
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = format;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		hr = m_device->CreateTexture2D(&desc, nullptr, &texture);
		RETURN_IF_FAILED(hr);

		textureview.CreateViews(m_device.Get(), texture);
	}
}

HRESULT AmbientLight::CreateOffscreen(DXGI_FORMAT format)
{
	HRESULT hr = S_OK;
	RecreateTexture(format, m_gameWidth, m_gameHeight, m_gameTexture);
	UINT thumb_width = m_blurSize;
	UINT thumb_height = m_blurSize;

	RecreateTexture(format, thumb_width, thumb_height, m_offscreen1);
	RecreateTexture(format, thumb_width, thumb_height, m_offscreen2);

	RecreateTexture(format, m_windowWidth, m_windowHeight, m_offscreen3);
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

		// Create source texture (in this example, we create a simple texture)
		m_context->CopySubresourceRegion(m_gameTexture.GetTexture(), 0, 0, 0, 0, desktopTexture.Get(), 0, &game_box);
		//m_copy.Apply(m_gameTexture_thumb, m_gameTexture);
		m_copy.Apply(m_offscreen1, m_gameTexture);

		int blur_passes = 2;
		for (int i = 0; i < blur_passes; i++)
		{
			m_blur.Apply(m_offscreen2, m_offscreen1, Blur::BlurDirection::BlurHorizontal);
			m_blur.Apply(m_offscreen1, m_offscreen2, Blur::BlurDirection::BlurVertical);
		}

		m_copy.Apply(m_offscreen3, m_offscreen1);
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

	m_context->CopySubresourceRegion(backBuffer.Get(), 0, dst_left.left, dst_left.top, 0, m_offscreen3.GetTexture(), 0, &dst_left);
	m_context->CopySubresourceRegion(backBuffer.Get(), 0, dst_right.left, dst_right.top, 0, m_offscreen3.GetTexture(), 0, &dst_right);

	m_swapchain->Present(1, 0);
}

