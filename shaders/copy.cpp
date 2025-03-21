#include "copy.h"
#include "d3dcompiler.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

using namespace DirectX;

const char* COPY_PS = R"(

Texture2D txSource : register(t0);
SamplerState samLinear : register(s0);

float4 main(float4 Pos : SV_POSITION, float2 Tex : TEXCOORD0) : SV_Target
{
    // Apply bilinear sampling when reading from the source texture
    return txSource.Sample(samLinear, Tex);
}

float4 main_hflip(float4 Pos : SV_POSITION, float2 Tex : TEXCOORD0) : SV_Target
{
    // Apply bilinear sampling when reading from the source texture
    return txSource.Sample(samLinear, float2(1.0 - Tex.x, Tex.y));
}

float4 main_vflip(float4 Pos : SV_POSITION, float2 Tex : TEXCOORD0) : SV_Target
{
    // Apply bilinear sampling when reading from the source texture
    return txSource.Sample(samLinear, float2(Tex.x, 1.0 - Tex.y));
}
)";

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

    ID3DBlob* errorBlob = nullptr;

    // Compile and create pixel shader
    ID3DBlob* psBlob = nullptr;
    hr = D3DCompile(COPY_PS, strlen(COPY_PS), "PS", nullptr, nullptr, "main", "ps_4_0", 0, 0, &psBlob, &errorBlob);
    RETURN_IF_FAILED(hr);

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader);
    psBlob->Release();
    RETURN_IF_FAILED(hr);

    hr = D3DCompile(COPY_PS, strlen(COPY_PS), "PS", nullptr, nullptr, "main_hflip", "ps_4_0", 0, 0, &psBlob, &errorBlob);
    RETURN_IF_FAILED(hr);

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader_hflip);
    psBlob->Release();
    RETURN_IF_FAILED(hr);

    hr = D3DCompile(COPY_PS, strlen(COPY_PS), "PS", nullptr, nullptr, "main_vflip", "ps_4_0", 0, 0, &psBlob, &errorBlob);
    RETURN_IF_FAILED(hr);

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader_vflip);
    psBlob->Release();
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

    return hr;
}

HRESULT Copy::Render(TextureView target, TextureView source, Flip flip)
{
    HRESULT hr = S_OK;

    D3D11_TEXTURE2D_DESC target_desc = {};
    target.GetTexture()->GetDesc(&target_desc);

    D3D11_VIEWPORT vp;
    vp.Width = (float)target_desc.Width;
    vp.Height = (float)target_desc.Height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    m_context->RSSetViewports(1, &vp);

    ID3D11RenderTargetView* rtv = target.GetRTV();
    const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    m_context->ClearRenderTargetView(rtv, clearColor);

    m_context->OMSetRenderTargets(1, &rtv, nullptr);

    switch (flip)
    {
    case FlipHorizontal:
        m_context->PSSetShader(m_pixelShader_hflip.Get(), nullptr, 0);
        break;
    case FlipVertical:
        m_context->PSSetShader(m_pixelShader_vflip.Get(), nullptr, 0);
        break;
    default:
        m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    }

    m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

    ID3D11ShaderResourceView* srv = source.GetSRV();
    m_context->PSSetShaderResources(0, 1, &srv);

    m_context->Draw(4, 0);

    rtv = nullptr;
    m_context->OMSetRenderTargets(1, &rtv, nullptr);
    srv = nullptr;
    m_context->PSSetShaderResources(0, 1, &srv);

    return S_OK;
}
