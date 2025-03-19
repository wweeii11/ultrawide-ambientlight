#pragma once
#include "../common.h"
#include <stdint.h>
#include "DirectXMath.h"


class Copy
{
public:
    enum Flip
    {
        FlipNone = 0,
        FlipHorizontal = 1,
        FlipVertical = 2
    };

    Copy();
    ~Copy();
    HRESULT Initialize(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context);
    HRESULT Apply(TextureView target, TextureView source, Flip flip = FlipNone);
private:
    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;

    ComPtr<ID3D11VertexShader> m_vertexShader;
    ComPtr<ID3D11PixelShader> m_pixelShader;
    ComPtr<ID3D11PixelShader> m_pixelShader_hflip;
    ComPtr<ID3D11PixelShader> m_pixelShader_vflip;
    ComPtr<ID3D11SamplerState> m_samplerState;
    ComPtr<ID3D11Buffer> m_vertexBuffer;
    ComPtr<ID3D11InputLayout> m_vertexLayout;
};
