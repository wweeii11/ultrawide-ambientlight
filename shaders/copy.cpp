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
    // Register 0: [x, y, z, w] -> 16 bytes
    DirectX::XMFLOAT2 srcOffset;
    DirectX::XMFLOAT2 srcSize;

    // Register 1: [x, y, z, w] -> 16 bytes
    DirectX::XMFLOAT2 dstOffset;
    DirectX::XMFLOAT2 dstSize;

    // Register 2: [x, y, z, w] -> 16 bytes
    uint32_t flipHorizontal;
    uint32_t flipVertical;
    float    padding[2];           // Manual padding to fill the 4-component register
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
    if (m_device != device)
    {
        m_shader = nullptr;
    }

    m_device = device;
    m_context = context;

    if (!m_shader)
    {
        hr = device->CreateComputeShader(g_copy, sizeof(g_copy), nullptr, &m_shader);
        RETURN_IF_FAILED(hr);

        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
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
    }

    return hr;
}

HRESULT Copy::Render(ID3D11DeviceContext* context, TextureView target, UINT targetOffsetX, UINT targetOffsetY, UINT targetWidth, UINT targetHeight,
    TextureView source, UINT sourceOffsetX, UINT sourceOffsetY, UINT sourceWidth, UINT sourceHeight,
    Flip flip)
{
    HRESULT hr = S_OK;

    if (!target.GetTexture() || !source.GetTexture())
        return E_FAIL;

    D3D11_TEXTURE2D_DESC target_desc = {};
    target.GetTexture()->GetDesc(&target_desc);

    context->CSSetShader(m_shader.Get(), nullptr, 0);

    ID3D11UnorderedAccessView* uav = target.GetUAV();
    context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);

    COPY_PARAMETERS copyParams = {};

    switch (flip)
    {
    case FlipHorizontal:
        copyParams.flipHorizontal = 1;
        break;
    case FlipVertical:
        copyParams.flipVertical = 2;
        break;
    }

    copyParams.srcOffset = { static_cast<float>(sourceOffsetX), static_cast<float>(sourceOffsetY) };
    copyParams.srcSize = { static_cast<float>(sourceWidth), static_cast<float>(sourceHeight) };
    copyParams.dstOffset = { static_cast<float>(targetOffsetX), static_cast<float>(targetOffsetY) };
    copyParams.dstSize = { static_cast<float>(targetWidth), static_cast<float>(targetHeight) };

    context->UpdateSubresource(m_params.Get(), 0, nullptr, &copyParams, sizeof(COPY_PARAMETERS), 0);
    context->CSSetConstantBuffers(0, 1, m_params.GetAddressOf());

    context->CSSetSamplers(0, 1, m_samplerState.GetAddressOf());

    ID3D11ShaderResourceView* srv = source.GetSRV();
    context->CSSetShaderResources(0, 1, &srv);

    context->Dispatch(
        (targetWidth + 15) / 16,
        (targetHeight + 15) / 16,
        1);

    uav = nullptr;
    context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
    srv = nullptr;
    context->CSSetShaderResources(0, 1, &srv);

    return S_OK;
}

HRESULT Copy::Render(ID3D11DeviceContext* context, TextureView target, TextureView source, Flip flip)
{
    D3D11_TEXTURE2D_DESC source_desc = {};
    source.GetTexture()->GetDesc(&source_desc);

    D3D11_TEXTURE2D_DESC target_desc = {};
    target.GetTexture()->GetDesc(&target_desc);

    return Render(context, target, 0, 0, target_desc.Width, target_desc.Height,
        source, 0, 0, source_desc.Width, source_desc.Height,
        flip);
}
