#include "vignette.h"
#include "d3dcompiler.h"
#include "vignette_bin.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

using namespace DirectX;

__declspec(align(16))
struct VIGNETTE_PARAMETERS
{
    XMFLOAT2 center;
    float intensity;
    float radius;
    float smoothness;
    float screenAspect;
    XMFLOAT4 vignetteColor;
};

Vignette::Vignette()
{
}

Vignette::~Vignette()
{
}

HRESULT Vignette::Initialize(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context, float intensity, float radius, float smoothness, float aspect)
{
    HRESULT hr = S_OK;
    if (m_device != device)
    {
        m_shader = nullptr;
    }

    m_device = device;
    m_context = context;

    if (!m_shader)
    {
        hr = device->CreateComputeShader(g_vignette, sizeof(g_vignette), nullptr, &m_shader);
        RETURN_IF_FAILED(hr);

        // Create constant buffer
        D3D11_BUFFER_DESC bufferDesc = {};
        bufferDesc.Usage = D3D11_USAGE_DEFAULT;
        bufferDesc.ByteWidth = sizeof(VIGNETTE_PARAMETERS);
        bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        hr = device->CreateBuffer(&bufferDesc, nullptr, &m_params);
        RETURN_IF_FAILED(hr);
    }

    VIGNETTE_PARAMETERS paramData = {};
    paramData.center = { 0.5f, 0.5f };
    paramData.intensity = intensity;
    paramData.radius = radius;
    paramData.smoothness = smoothness;
    paramData.screenAspect = aspect;
    paramData.vignetteColor = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_context->UpdateSubresource(m_params.Get(), 0, nullptr, &paramData, sizeof(VIGNETTE_PARAMETERS), 0);

    return hr;
}

HRESULT Vignette::Render(ID3D11DeviceContext* context, TextureView target)
{
    HRESULT hr = S_OK;

    if (!target.GetTexture())
        return E_FAIL;

    D3D11_TEXTURE2D_DESC target_desc = {};
    target.GetTexture()->GetDesc(&target_desc);

    context->CSSetConstantBuffers(0, 1, m_params.GetAddressOf());

    context->CSSetShader(m_shader.Get(), nullptr, 0);

    ID3D11UnorderedAccessView* uav = target.GetUAV();
    context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);

    context->Dispatch(
        (target_desc.Width + 15) / 16,
        (target_desc.Height + 15) / 16,
        1);

    uav = nullptr;
    context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);

    return S_OK;
}
