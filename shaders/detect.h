#pragma once
#include "../common.h"
#include <stdint.h>
#include "DirectXMath.h"
#include <memory>
#include <vector>

using namespace DirectX;

class Detection
{
public:
    Detection();
    ~Detection();

    HRESULT Initialize(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context, UINT width, UINT height,
        float blackThreshold, float blackRatio);

    HRESULT Detect(ID3D11DeviceContext* context, TextureView target);
    HRESULT RenderMask(ID3D11DeviceContext* context, TextureView target);

    std::vector<D3D11_BOX> GetDetectedBoxes();
    std::vector<D3D11_BOX> GetFixedBoxes(UINT gameWidth, UINT gameHeight);

private:
    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;

    ComPtr<ID3D11ComputeShader> m_lumaShader;
    ComPtr<ID3D11ComputeShader> m_maskShader;

    TextureView m_luma;
    ComPtr<ID3D11Texture2D> m_lumaStaging;

    float m_blackThreshold;
    float m_blackRatio;

    // captured texture size
    UINT m_width, m_height;

    // detected black bar sizes
    UINT m_topBar, m_bottomBar;
    UINT m_leftBar, m_rightBar;

    // detected non-black area
    UINT m_detectWidth, m_detectHeight;
};
