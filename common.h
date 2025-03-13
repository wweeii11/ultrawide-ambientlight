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
