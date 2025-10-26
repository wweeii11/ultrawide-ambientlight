#include "blur.h"
#include "d3dcompiler.h"
#include "blur_bin.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

using namespace DirectX;
using namespace DirectX::SimpleMath;

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

__declspec(align(16))
struct VS_BLUR_PARAMETERS
{
    static constexpr size_t MAX_SAMPLE_COUNT = 63;

    XMFLOAT4 sampleCount = { 5,0,0,0 };
    XMFLOAT4 sampleOffsets[MAX_SAMPLE_COUNT];
    XMFLOAT4 sampleWeights[MAX_SAMPLE_COUNT];

    void SetBlurEffectParameters(float dx, float dy, size_t samples, const VS_BLOOM_PARAMETERS& params)
    {
        sampleCount.x = (float)max(min(samples, MAX_SAMPLE_COUNT), 1);

        sampleWeights[0].x = ComputeGaussian(0, params.blurAmount);
        sampleOffsets[0].x = sampleOffsets[0].y = 0.f;

        float totalWeights = sampleWeights[0].x;

        // Add pairs of additional sample taps, positioned
        // along a line in both directions from the center.
        for (size_t i = 0; i < samples / 2; i++)
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

        for (size_t i = 0; i < samples; i++)
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

HRESULT Blur::Initialize(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context, UINT width, UINT height, UINT samples)
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
        hr = device->CreateComputeShader(g_blur, sizeof(g_blur), nullptr, &m_shader);
        RETURN_IF_FAILED(hr);

        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_MIRROR;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_MIRROR;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_MIRROR;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        hr = device->CreateSamplerState(&samplerDesc, &m_samplerState);

        // Create constant buffer
        D3D11_BUFFER_DESC bufferDesc = {};
        bufferDesc.Usage = D3D11_USAGE_DEFAULT;
        bufferDesc.ByteWidth = sizeof(VS_BLUR_PARAMETERS);
        bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        hr = device->CreateBuffer(&bufferDesc, nullptr, &m_blurParamsWidth);
        hr = device->CreateBuffer(&bufferDesc, nullptr, &m_blurParamsHeight);

        // Create temporary texture
        m_tempTexture.Clear();
    }

    VS_BLUR_PARAMETERS blurData;
    float dx = 1.0f / (float)width;
    float dy = 1.0f / (float)height;
    blurData.SetBlurEffectParameters(dx, 0, samples, g_BloomPresets[g_Bloom]);
    m_context->UpdateSubresource(m_blurParamsWidth.Get(), 0, nullptr, &blurData, sizeof(VS_BLUR_PARAMETERS), 0);

    blurData.SetBlurEffectParameters(0, dy, samples, g_BloomPresets[g_Bloom]);
    m_context->UpdateSubresource(m_blurParamsHeight.Get(), 0, nullptr, &blurData, sizeof(VS_BLUR_PARAMETERS), 0);

    return S_OK;
}

HRESULT Blur::Render(ID3D11DeviceContext* context, TextureView target, UINT passes)
{
    HRESULT hr = S_OK;

    if (!target.GetTexture())
        return E_FAIL;

    D3D11_TEXTURE2D_DESC target_desc = {};
    target.GetTexture()->GetDesc(&target_desc);

    m_tempTexture.RecreateTexture(m_device.Get(), target_desc.Format, target_desc.Width, target_desc.Height);

    for (UINT i = 0; i < passes; i++)
    {
        DoBlurPass(context, m_tempTexture, target, BlurHorizontal);
        DoBlurPass(context, target, m_tempTexture, BlurVertical);
    }

    return hr;
}

HRESULT Blur::DoBlurPass(ID3D11DeviceContext* context, TextureView target, TextureView source, BlurDirection direction)
{
    HRESULT hr = S_OK;

    D3D11_TEXTURE2D_DESC target_desc = {};
    target.GetTexture()->GetDesc(&target_desc);

    // Update the constant buffer
    if (direction == BlurHorizontal)
        context->CSSetConstantBuffers(0, 1, m_blurParamsWidth.GetAddressOf());
    else
        context->CSSetConstantBuffers(0, 1, m_blurParamsHeight.GetAddressOf());

    context->CSSetShader(m_shader.Get(), nullptr, 0);

    context->CSSetSamplers(0, 1, m_samplerState.GetAddressOf());

    ID3D11ShaderResourceView* srv = source.GetSRV();
    context->CSSetShaderResources(0, 1, &srv);

    ID3D11UnorderedAccessView* uav = target.GetUAV();
    context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);

    context->Dispatch(
        (target_desc.Width + 15) / 16,
        (target_desc.Height + 15) / 16,
        1);

    uav = nullptr;
    context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
    srv = nullptr;
    context->CSSetShaderResources(0, 1, &srv);

    return S_OK;
}
