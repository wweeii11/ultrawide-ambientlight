﻿#pragma once

#include "common.h"
#include "capture.h"
#include "dcomp.h"
#include "shaders/copy.h"
#include "shaders/blur.h"



class AmbientLight
{
public:
	AmbientLight(const AppSettings& settings);
	~AmbientLight();

	HRESULT Initialize(HWND hwnd);
	void Render();

private:
	HWND m_hwnd;
	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11DeviceContext> m_context;
	ComPtr<IDXGISwapChain1> m_swapchain;
	ComPtr<IDCompositionDevice> m_dcompDevice;
	ComPtr<IDCompositionTarget> m_dcompTarget;
	ComPtr<IDCompositionVisual> m_dcompVisual;

	DesktopCapture m_capture;
	Blur m_blurDownscale;
	Blur m_blurPre;
	Copy m_copy;

	UINT m_gameWidth;
	UINT m_gameHeight;
	UINT m_windowWidth;
	UINT m_windowHeight;
	UINT m_effectWidth;
	UINT m_effectHeight;
	UINT m_effectZoom;
	UINT m_blurSize;
	UINT m_blurPasses;
	bool m_mirror;
	UINT m_updateInterval;

	// render effect at top/bottom of screen instead of left/right
	bool m_topbottom;

	TextureView m_gameTexture;
	TextureView m_offscreen1;
	TextureView m_offscreen3;

	HRESULT CreateOffscreen(DXGI_FORMAT format);

	void RenderEffects();
	void Present();
};
