#include "vignette.h"
#include "d3dcompiler.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

using namespace DirectX;

const char* VIGNETTE_PS = R"(

cbuffer VignetteParams : register(b0)
{
    float2 center;       // Center of vignette effect (normalized, default 0.5, 0.5)
    float intensity;     // Intensity of the vignette effect (0.0 - 1.0)
    float radius;        // Radius where the effect begins (0.0 - 1.0)
    float smoothness;    // Controls the smoothness of the transition (0.0 - 1.0)
    float screenAspect;  // Aspect ratio of the screen
    float4 vignetteColor; // Color of the vignette (usually black)
};

Texture2D inputTexture : register(t0);
SamplerState samplerState : register(s0);

float4 main(float4 pos : SV_POSITION, float2 texCoord : TEXCOORD0) : SV_Target0
{
    // Sample the original texture
    float4 originalColor = inputTexture.Sample(samplerState, texCoord);
    
    float2 adjustedCoords = texCoord;
    if (screenAspect >= 1.0)
        adjustedCoords.y = (adjustedCoords.y - center.y) / screenAspect + center.y;
    else
        adjustedCoords.x = (adjustedCoords.x - center.x) / screenAspect + center.x;

    // Calculate distance from center
    float2 distFromCenter = adjustedCoords - center;
    float dist = length(distFromCenter);
    
    // Calculate vignette factor
    float vignetteFactor = smoothstep(radius, radius - smoothness, dist);
    
    // Apply intensity
    vignetteFactor = 1.0 - ((1.0 - vignetteFactor) * intensity);
    
    // Mix original color with vignette color
    float4 finalColor = lerp(vignetteColor, originalColor, vignetteFactor);
    
    return finalColor;
    //return vignetteColor;
    //return float4(0.5f, 0.5f, 0.5f, 0.0f);
}

)";

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
    m_device = device;
    m_context = context;

    ID3DBlob* errorBlob = nullptr;

    // Compile and create pixel shader
    ID3DBlob* psBlob = nullptr;
    hr = D3DCompile(VIGNETTE_PS, strlen(VIGNETTE_PS), "PS", nullptr, nullptr, "main", "ps_4_0", 0, 0, &psBlob, &errorBlob);
    RETURN_IF_FAILED(hr);

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader);
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
    RETURN_IF_FAILED(hr);

    // Create constant buffer
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.ByteWidth = sizeof(VIGNETTE_PARAMETERS);
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    hr = device->CreateBuffer(&bufferDesc, nullptr, &m_params);
    RETURN_IF_FAILED(hr);

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

HRESULT Vignette::Render(TextureView target, TextureView source)
{
    HRESULT hr = S_OK;

    if (!target.GetTexture() || !source.GetTexture())
        return E_FAIL;

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

    m_context->PSSetConstantBuffers(0, 1, m_params.GetAddressOf());

    ID3D11RenderTargetView* rtv = target.GetRTV();

    m_context->OMSetRenderTargets(1, &rtv, nullptr);

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
