#pragma once
#include "../common.h"
#include <stdint.h>
#include "DirectXMath.h"
#include <memory>
#include <vector>
#include "dxgi1_6.h"

using namespace DirectX;

class Detection
{
public:
    Detection();
    ~Detection();

    HRESULT Initialize(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context, UINT width, UINT height,
        float blackThreshold, float blackRatio, bool symmetricBars, UINT reservedWidth, UINT reservedHeight, DXGI_COLOR_SPACE_TYPE colorSpace);
    
    HRESULT Detect(ID3D11DeviceContext* context, TextureView target);
    HRESULT RenderLumaMask(ID3D11DeviceContext* context, TextureView target);

    std::vector<BlackBar> GetDetectedBars();
    static std::vector<BlackBar> GetFixedBars(UINT windowWidth, UINT windowHeight, UINT gameWidth, UINT gameHeight);

private:
    HRESULT CreateBuffers();
    HRESULT DispatchLuma(ID3D11DeviceContext* context, TextureView target);
    HRESULT DispatchRowColAnalysis(ID3D11DeviceContext* context);
    HRESULT FetchRowResults(ID3D11DeviceContext* pContext);
    HRESULT FetchColResults(ID3D11DeviceContext* pContext);
    

    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;

    ComPtr<ID3D11ComputeShader> m_lumaShader;
    ComPtr<ID3D11ComputeShader> m_lumaHDR10Shader;
    ComPtr<ID3D11ComputeShader> m_lumaSCRGBShader;

    ComPtr<ID3D11ComputeShader> m_rowAnalysisShader;
    ComPtr<ID3D11ComputeShader> m_colAnalysisShader;

    ComPtr<ID3D11ComputeShader> m_lumaMaskShader;
    ComPtr<ID3D11ComputeShader> m_lumaMaskShaderUNorm;

    TextureView m_luma;
    ComPtr<ID3D11Texture2D> m_lumaStaging;

    ComPtr<ID3D11Buffer> m_rowResults;
    ComPtr<ID3D11UnorderedAccessView> m_rowResultsUAV;
    ComPtr<ID3D11Buffer> m_rowStaging;

    ComPtr<ID3D11Buffer> m_colResults;
    ComPtr<ID3D11UnorderedAccessView> m_colResultsUAV;
    ComPtr<ID3D11Buffer> m_colStaging;

    ComPtr<ID3D11Buffer> m_constants;

    float m_blackThreshold;
    float m_blackRatio;

    bool m_symmetricBars;

    // captured texture size
    UINT m_width, m_height;

    // detected black bar sizes
    UINT m_topBar, m_bottomBar;
    UINT m_leftBar, m_rightBar;
    UINT m_reservedWidth;
    UINT m_reservedHeight;

    DXGI_COLOR_SPACE_TYPE m_colorSpace;
};
