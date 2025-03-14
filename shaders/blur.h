#pragma once
#include "../common.h"
#include <stdint.h>
#include "DirectXMath.h"
#include <memory>

using namespace DirectX;

class Blur
{
public:
	enum BlurDirection
	{
		BlurHorizontal = 0,
		BlurVertical = 1
	};

	Blur();
	~Blur();
	HRESULT Initialize(ComPtr<ID3D11Device> device, UINT width, UINT height);
	HRESULT Apply(TextureView target, TextureView source, BlurDirection direction);
private:
	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11DeviceContext> m_context;

	ComPtr<ID3D11VertexShader> m_vertexShader;
	ComPtr<ID3D11PixelShader> m_pixelShader;
	ComPtr<ID3D11SamplerState> m_samplerState;
	ComPtr<ID3D11Buffer> m_vertexBuffer;
	ComPtr<ID3D11InputLayout> m_vertexLayout;

	ComPtr<ID3D11Buffer>             m_blurParamsWidth;
	ComPtr<ID3D11Buffer>             m_blurParamsHeight;

};
