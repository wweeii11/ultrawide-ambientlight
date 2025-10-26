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
    HRESULT Render(ID3D11DeviceContext* context, TextureView target);
private:
    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;

    ComPtr<ID3D11ComputeShader> m_shader;

    ComPtr<ID3D11Buffer> m_params;
};
