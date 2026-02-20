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
    HRESULT Render(ID3D11DeviceContext* context, TextureView target, TextureView source, Flip flip = FlipNone);
    HRESULT Render(ID3D11DeviceContext* context, TextureView target, UINT targetOffsetX, UINT targetOffsetY, UINT targetWidth, UINT targetHeight,
                                                 TextureView source, UINT sourceOffsetX, UINT sourceOffsetY, UINT sourceWidth, UINT sourceHeight, 
                                                 Flip flip = FlipNone);
private:
    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;

    
    ComPtr<ID3D11ComputeShader> m_shader;
    ComPtr<ID3D11Buffer>        m_params;
    ComPtr<ID3D11SamplerState> m_samplerState;

};
