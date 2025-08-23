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

    ComPtr<ID3D11Buffer> m_rowBuf;
    ComPtr<ID3D11Buffer> m_rowStaging;
    ComPtr<ID3D11UnorderedAccessView> m_rowUAV;
    ComPtr<ID3D11Buffer> m_colBuf;
    ComPtr<ID3D11Buffer> m_colStaging;
    ComPtr<ID3D11UnorderedAccessView> m_colUAV;

    ComPtr<ID3D11Buffer> m_detectCB;

    int m_width, m_height;
    int m_topBarEnd, m_bottomBarStart;
    int m_leftBarEnd, m_rightBarStart;
    int m_detectWidth, m_detectHeight;
};
