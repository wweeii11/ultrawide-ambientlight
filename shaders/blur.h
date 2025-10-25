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
    HRESULT Initialize(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context, UINT width, UINT height, UINT samples);
    HRESULT Render(ID3D11DeviceContext* context, TextureView target, UINT passes);
private:
    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;

    ComPtr<ID3D11ComputeShader> m_shader;
    ComPtr<ID3D11SamplerState> m_samplerState;

    ComPtr<ID3D11Buffer> m_blurParamsWidth;
    ComPtr<ID3D11Buffer> m_blurParamsHeight;

    TextureView m_tempTexture;

    HRESULT DoBlurPass(ID3D11DeviceContext* context, TextureView target, TextureView source, BlurDirection direction);
};
