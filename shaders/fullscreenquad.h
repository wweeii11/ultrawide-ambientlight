#pragma once
#include "../common.h"
#include <stdint.h>
#include "DirectXMath.h"

class FullScreenQuad
{
public:
	FullScreenQuad();
	~FullScreenQuad();

	HRESULT Initialize(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context);
	HRESULT Render(ID3D11DeviceContext* context);

private:
	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11DeviceContext> m_context;

	ComPtr<ID3D11VertexShader> m_vertexShader;
	ComPtr<ID3D11Buffer> m_vertexBuffer;
	ComPtr<ID3D11InputLayout> m_vertexLayout;
};