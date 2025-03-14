#pragma once
#include "../common.h"
#include <stdint.h>
#include "DirectXMath.h"


class Copy
{
public:
	Copy();
	~Copy();
	HRESULT Initialize(ComPtr<ID3D11Device> device);
	HRESULT Apply(TextureView target, TextureView source, bool hflip = false);
private:
	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11DeviceContext> m_context;

	ComPtr<ID3D11VertexShader> m_vertexShader;
	ComPtr<ID3D11PixelShader> m_pixelShader;
	ComPtr<ID3D11PixelShader> m_pixelShader_hflip;
	ComPtr<ID3D11SamplerState> m_samplerState;
	ComPtr<ID3D11Buffer> m_vertexBuffer;
	ComPtr<ID3D11InputLayout> m_vertexLayout;
};
