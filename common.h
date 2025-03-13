#pragma once

#include "windows.h"
#include "d3d11.h"
#include "dxgi1_2.h"
#include "dxgi1_3.h"
#include "wrl/client.h"
#include "SimpleMath.h"
#include <string>

using namespace Microsoft::WRL;

#define RETURN_IF_FAILED(hr) if (FAILED(hr)) return hr;

#define RECT_WIDTH(r) (r.right - r.left)
#define RECT_HEIGHT(r) (r.bottom - r.top)

struct AppSettings
{
	UINT gameWidth;
	UINT gameHeight;
	UINT blurDownscale;
	UINT blurPasses;
	UINT updateInterval;
	bool mirrored;
};

class TextureView
{
public:
	TextureView()
	{

	}
	void CreateViews(ID3D11Device* device, ID3D11Texture2D* texture)
	{
		m_texture = texture;

		if (texture && device)
		{
			D3D11_TEXTURE2D_DESC desc;
			texture->GetDesc(&desc);
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = desc.Format;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = desc.MipLevels;
			srvDesc.Texture2D.MostDetailedMip = 0;
			device->CreateShaderResourceView(texture, &srvDesc, &m_srv);

			device->CreateRenderTargetView(texture, nullptr, &m_rtv);
		}
	}
	void Clear()
	{
		m_texture = nullptr;
		m_srv = nullptr;
		m_rtv = nullptr;
	}
	HRESULT RecreateTexture(ID3D11Device* device, DXGI_FORMAT format, UINT width, UINT height)
	{
		HRESULT hr = S_OK;
		if (GetTexture())
		{
			ID3D11Texture2D* texture = GetTexture();
			D3D11_TEXTURE2D_DESC desc;
			texture->GetDesc(&desc);

			if (desc.Format != format || desc.Width != width || desc.Height != height)
			{
				Clear();
			}
		}
		if (!GetTexture())
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
			hr = device->CreateTexture2D(&desc, nullptr, &texture);
			CreateViews(device, texture);
		}
		return hr;
	}

	ID3D11Texture2D* GetTexture() const
	{
		return m_texture.Get();
	}
	ID3D11ShaderResourceView* GetSRV() const
	{
		return m_srv.Get();
	}
	ID3D11RenderTargetView* GetRTV() const
	{
		return m_rtv.Get();
	}
private:
	ComPtr<ID3D11Texture2D> m_texture;
	ComPtr<ID3D11ShaderResourceView> m_srv;
	ComPtr<ID3D11RenderTargetView> m_rtv;
};
