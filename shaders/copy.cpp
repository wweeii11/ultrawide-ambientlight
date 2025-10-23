#include "copy.h"
#include "d3dcompiler.h"
#include "copy_bin.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

using namespace DirectX;

__declspec(align(16))
struct COPY_PARAMETERS
{
    UINT flipMode = 0; // 0=normal, 1=HFlip, 2=VFlip
    XMFLOAT2 padding = {}; // keep 16-byte alignment
};

Copy::Copy()
{
}

Copy::~Copy()
{
}

HRESULT Copy::Initialize(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context)
{
    HRESULT hr = S_OK;
    m_device = device;
    m_context = context;

    hr = device->CreateComputeShader(g_copy, sizeof(g_copy), nullptr, &m_shader);
    RETURN_IF_FAILED(hr);

    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = device->CreateSamplerState(&samplerDesc, &m_samplerState);

    // Create constant buffer
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.ByteWidth = sizeof(COPY_PARAMETERS);
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    hr = device->CreateBuffer(&bufferDesc, nullptr, &m_params);

    return hr;
}

HRESULT Copy::Render(TextureView target, TextureView source, Flip flip)
{
    HRESULT hr = S_OK;

    if (!target.GetTexture() || !source.GetTexture())
		return E_FAIL;

    D3D11_TEXTURE2D_DESC target_desc = {};
    target.GetTexture()->GetDesc(&target_desc);

    m_context->CSSetShader(m_shader.Get(), nullptr, 0);

    ID3D11UnorderedAccessView* uav = target.GetUAV();
    m_context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);

    COPY_PARAMETERS copyParams = {};

    switch (flip)
    {
    case FlipHorizontal:
        copyParams.flipMode = 1;
        break;
    case FlipVertical:
        copyParams.flipMode = 2;
        break;
    default:
        copyParams.flipMode = 0;
    }
    m_context->UpdateSubresource(m_params.Get(), 0, nullptr, &copyParams, sizeof(COPY_PARAMETERS), 0);
    m_context->CSSetConstantBuffers(0, 1, m_params.GetAddressOf());

    m_context->CSSetSamplers(0, 1, m_samplerState.GetAddressOf());

    ID3D11ShaderResourceView* srv = source.GetSRV();
    m_context->CSSetShaderResources(0, 1, &srv);

    m_context->Dispatch(
        (target_desc.Width + 15) / 16,
        (target_desc.Height + 15) / 16,
        1);

    uav = nullptr;
    m_context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
    srv = nullptr;
    m_context->CSSetShaderResources(0, 1, &srv);

    return S_OK;
}
