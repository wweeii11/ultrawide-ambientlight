#pragma once
#include "../common.h"
#include <stdint.h>
#include "DirectXMath.h"
#include <memory>

using namespace DirectX;

class Detection
{
public:
    Detection();
    ~Detection();
    HRESULT Initialize(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context, UINT width, UINT height,
        float blackThreshold, float blackRatio);
    HRESULT Detect(TextureView target);

    int GetWidth();
    int GetHeight();
private:
    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;

    ComPtr<ID3D11ComputeShader> m_computeShader;

    ComPtr<ID3D11Texture2D> m_luma;
    ComPtr<ID3D11Texture2D> m_lumaStaging;
    ComPtr<ID3D11UnorderedAccessView> m_lumaUAV;

    float m_blackThreshold;
    float m_blackRatio;
    int m_width, m_height;
    int m_topBarEnd, m_bottomBarStart;
    int m_leftBarEnd, m_rightBarStart;
    int m_detectWidth, m_detectHeight;
};
