#include "blur.h"
#include "d3dcompiler.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

using namespace DirectX;
using namespace DirectX::SimpleMath;

const char* BLUR_VS = R"(

struct VS_INPUT {
    float3 Pos : POSITION;
    float2 Tex : TEXCOORD0;
};

struct PS_INPUT {
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
};

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    output.Pos = float4(input.Pos, 1.0f);
    output.Tex = input.Tex;
    return output;
}

)";

const char* BLUR_PS = R"(

Texture2D<float4> Texture : register(t0);
sampler TextureSampler : register(s0);

#define SAMPLE_COUNT 5

cbuffer VS_BLUR_PARAMETERS : register(b0)
{
    float2 SampleOffsets[SAMPLE_COUNT];
    float SampleWeights[SAMPLE_COUNT];
}

float4 main(float4 color : SV_POSITION, float2 texCoord : TEXCOORD0) : SV_Target0
{
    float4 c = 0;

    // Combine a number of weighted image filter taps.
    for (int i = 0; i < SAMPLE_COUNT; i++)
    {
        c += Texture.Sample(TextureSampler, texCoord + SampleOffsets[i]) * SampleWeights[i];
    }

    return c;
}

)";


// Vertex structure
struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT2 texCoord;
};

struct VS_BLOOM_PARAMETERS
{
    float bloomThreshold;
    float blurAmount;
    float bloomIntensity;
    float baseIntensity;
    float bloomSaturation;
    float baseSaturation;
    uint8_t na[8];
};

struct VS_BLUR_PARAMETERS
{
    static constexpr size_t SAMPLE_COUNT = 5;

    XMFLOAT4 sampleOffsets[SAMPLE_COUNT];
    XMFLOAT4 sampleWeights[SAMPLE_COUNT];

    void SetBlurEffectParameters(float dx, float dy, const VS_BLOOM_PARAMETERS& params)
    {
        sampleWeights[0].x = ComputeGaussian(0, params.blurAmount);
        sampleOffsets[0].x = sampleOffsets[0].y = 0.f;

        float totalWeights = sampleWeights[0].x;

        // Add pairs of additional sample taps, positioned
        // along a line in both directions from the center.
        for (size_t i = 0; i < SAMPLE_COUNT / 2; i++)
        {
            // Store weights for the positive and negative taps.
            float weight = ComputeGaussian(float(i + 1.f), params.blurAmount);

            sampleWeights[i * 2 + 1].x = weight;
            sampleWeights[i * 2 + 2].x = weight;

            totalWeights += weight * 2;

            // To get the maximum amount of blurring from a limited number of
            // pixel shader samples, we take advantage of the bilinear filtering
            // hardware inside the texture fetch unit. If we position our texture
            // coordinates exactly halfway between two texels, the filtering unit
            // will average them for us, giving two samples for the price of one.
            // This allows us to step in units of two texels per sample, rather
            // than just one at a time. The 1.5 offset kicks things off by
            // positioning us nicely in between two texels.
            float sampleOffset = float(i) * 2.f + 1.5f;

            Vector2 delta = Vector2(dx, dy) * sampleOffset;

            // Store texture coordinate offsets for the positive and negative taps.
            sampleOffsets[i * 2 + 1].x = delta.x;
            sampleOffsets[i * 2 + 1].y = delta.y;
            sampleOffsets[i * 2 + 2].x = -delta.x;
            sampleOffsets[i * 2 + 2].y = -delta.y;
        }

        for (size_t i = 0; i < SAMPLE_COUNT; i++)
        {
            sampleWeights[i].x /= totalWeights;
        }
    }

private:
    float ComputeGaussian(float n, float theta)
    {
        return (float)((1.0 / sqrtf(2 * XM_PI * theta)) * expf(-(n * n) / (2 * theta * theta)));
    }
};

enum BloomPresets
{
    Default = 0,
    Soft,
    Desaturated,
    Saturated,
    Blurry,
    Subtle,
    None
};

BloomPresets g_Bloom = Default;

static const VS_BLOOM_PARAMETERS g_BloomPresets[] =
{
    //Thresh  Blur Bloom  Base  BloomSat BaseSat
    { 0.25f,  4,   1.25f, 1,    1,       1 }, // Default
    { 0,      3,   1,     1,    1,       1 }, // Soft
    { 0.5f,   8,   2,     1,    0,       1 }, // Desaturated
    { 0.25f,  4,   2,     1,    2,       0 }, // Saturated
    { 0,      2,   1,     0.1f, 1,       1 }, // Blurry
    { 0.5f,   2,   1,     1,    1,       1 }, // Subtle
    { 0.25f,  4,   1.25f, 1,    1,       1 }, // None
};

Blur::Blur()
{
}

Blur::~Blur()
{
}

HRESULT Blur::Initialize(ComPtr<ID3D11Device> device, UINT width, UINT height)
{
	HRESULT hr = S_OK;
	m_device = device;
	m_device->GetImmediateContext(&m_context);

    ID3DBlob* errorBlob = nullptr;

    // Compile and create vertex shader
    ID3DBlob* vsBlob = nullptr;
    hr = D3DCompile(BLUR_VS, strlen(BLUR_VS), "VS", nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, &errorBlob);
    RETURN_IF_FAILED(hr);
    hr = m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vertexShader);
    RETURN_IF_FAILED(hr);

    // Create input layout
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT numElements = ARRAYSIZE(layout);

    hr = device->CreateInputLayout(layout, numElements, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_vertexLayout);
    vsBlob->Release();
    RETURN_IF_FAILED(hr);

    // Compile and create pixel shader
    ID3DBlob* psBlob = nullptr;
    hr = D3DCompile(BLUR_PS, strlen(BLUR_PS), "PS", nullptr, nullptr, "main", "ps_4_0", 0, 0, &psBlob, &errorBlob);
    RETURN_IF_FAILED(hr);

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader);
    psBlob->Release();
    RETURN_IF_FAILED(hr);

    // Define a simple quad with texture coordinates
    Vertex quadVertices[] =
    {
        { XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3(-1.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(1.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(1.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) },
    };

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.ByteWidth = sizeof(quadVertices);
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = quadVertices;

    hr = device->CreateBuffer(&bufferDesc, &initData, &m_vertexBuffer);

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
    bufferDesc.ByteWidth = sizeof(VS_BLUR_PARAMETERS);
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    hr = device->CreateBuffer(&bufferDesc, nullptr, &m_blurParamsWidth);
    hr = device->CreateBuffer(&bufferDesc, nullptr, &m_blurParamsHeight);

    VS_BLUR_PARAMETERS blurData;
    float dx = 1.0f / (float)width;
    float dy = 1.0f / (float)height;
    blurData.SetBlurEffectParameters(dx, 0, g_BloomPresets[g_Bloom]);
    m_context->UpdateSubresource(m_blurParamsWidth.Get(), 0, nullptr, &blurData, sizeof(VS_BLUR_PARAMETERS), 0);

    blurData.SetBlurEffectParameters(0, dy, g_BloomPresets[g_Bloom]);
    m_context->UpdateSubresource(m_blurParamsHeight.Get(), 0, nullptr, &blurData, sizeof(VS_BLUR_PARAMETERS), 0);

    return S_OK;
}
HRESULT Blur::Apply(TextureView target, UINT passes)
{
    HRESULT hr = S_OK;

    D3D11_TEXTURE2D_DESC target_desc = {};
    target.GetTexture()->GetDesc(&target_desc);

    m_tempTexture.RecreateTexture(m_device.Get(), target_desc.Format, target_desc.Width, target_desc.Height);

    D3D11_VIEWPORT vp;
    vp.Width = (float)target_desc.Width;
    vp.Height = (float)target_desc.Height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    m_context->RSSetViewports(1, &vp);

    for (UINT i = 0; i < passes; i++)
    {
        DoBlurPass(m_tempTexture, target, BlurHorizontal);
        DoBlurPass(target, m_tempTexture, BlurVertical);
    }

    return hr;
}

HRESULT Blur::DoBlurPass(TextureView target, TextureView source, BlurDirection direction)
{
    HRESULT hr = S_OK;

    D3D11_TEXTURE2D_DESC target_desc = {};
    target.GetTexture()->GetDesc(&target_desc);

    // Update the constant buffer
    if (direction == BlurHorizontal)
        m_context->PSSetConstantBuffers(0, 1, m_blurParamsWidth.GetAddressOf());
    else
        m_context->PSSetConstantBuffers(0, 1, m_blurParamsHeight.GetAddressOf());

    ID3D11RenderTargetView* rtv = target.GetRTV();
    const float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_context->ClearRenderTargetView(rtv, clearColor);

    m_context->OMSetRenderTargets(1, &rtv, nullptr);

    // Set the vertex buffer
    m_context->IASetInputLayout(m_vertexLayout.Get());

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);

    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

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
