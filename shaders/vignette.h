#pragma once
#include "../common.h"
#include <stdint.h>
#include "DirectXMath.h"


class Vignette
{
public:
    Vignette();
    ~Vignette();
    HRESULT Initialize(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context, float intensity, float radius, float smoothness, float aspect);
    HRESULT Render(TextureView target, TextureView source);
private:
    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;

    ComPtr<ID3D11PixelShader> m_pixelShader;
    ComPtr<ID3D11SamplerState> m_samplerState;

    ComPtr<ID3D11Buffer> m_params;
};
