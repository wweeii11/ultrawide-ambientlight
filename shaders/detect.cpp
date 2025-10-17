#include "detect.h"
#include "d3dcompiler.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

using namespace DirectX;

// shader to compute luma for each pixel and output to UAV
const char* DETECT_CS = R"(
// Rec.709 luma weights
static const float3 LumaWeights = float3(0.2126, 0.7152, 0.0722);

Texture2D<float4> InputTexture : register(t0);
RWTexture2D<float> OutputTexture: register(u0);

float3 SRGBToLinear(float3 srgb)
{
    // Fast sRGB → Linear approx (could replace with exact curve if needed)
    return pow(srgb, 2.2);
}

// Thread group: each thread processes 1 row and 1 column
[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint width, height;
    InputTexture.GetDimensions(width, height);

    int2 coord = id.xy;
    if (coord.x >= width || coord.y >= height)
        return;

    float4 color = InputTexture.Load(int3(coord, 0));
    float luma = dot(color.rgb, LumaWeights);
    OutputTexture[coord] = luma;
}

)";

Detection::Detection()
{
}

Detection::~Detection()
{
}

static void CreateLumaMaskWithUAVAndStaging(
    ID3D11Device* dev,
    UINT width,
    UINT height,
    ID3D11Texture2D** gpuTexOut,           // UAV-capable DEFAULT buffer
    ID3D11UnorderedAccessView** uavOut, // UAV
    ID3D11Texture2D** stagingOut)          // STAGING copy for CPU read
{
    D3D11_TEXTURE2D_DESC textureDesc;
    ZeroMemory(&textureDesc, sizeof(textureDesc));

    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = 0;
    dev->CreateTexture2D(&textureDesc, nullptr, gpuTexOut);

    // UAV
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavd = {};
    uavd.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    uavd.Format = DXGI_FORMAT_R8_UNORM; // structured
    uavd.Texture2D.MipSlice = 0;
    
    dev->CreateUnorderedAccessView(*gpuTexOut, &uavd, uavOut);

    // Staging texture
    textureDesc.Usage = D3D11_USAGE_STAGING;
    textureDesc.BindFlags = 0;
    textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    dev->CreateTexture2D(&textureDesc, nullptr, stagingOut);
}

HRESULT Detection::Initialize(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context, UINT width, UINT height, float blackThreshold, float blackRatio)
{
    HRESULT hr = S_OK;
    m_device = device;
    m_context = context;

    ID3DBlob* errorBlob = nullptr;

    // Compile and create pixel shader
    ID3DBlob* csBlob = nullptr;
    hr = D3DCompile(DETECT_CS, strlen(DETECT_CS), "CS", nullptr, nullptr, "main", "cs_5_0", 0, 0, &csBlob, &errorBlob);
    RETURN_IF_FAILED(hr);

    hr = device->CreateComputeShader(csBlob->GetBufferPointer(), csBlob->GetBufferSize(), nullptr, &m_computeShader);
    csBlob->Release();
    RETURN_IF_FAILED(hr);

    CreateLumaMaskWithUAVAndStaging(
        device.Get(),
        width,
        height,
        &m_luma,
        &m_lumaUAV,
        &m_lumaStaging);

    m_width = width;
    m_height = height;
    m_blackThreshold = blackThreshold;
    m_blackRatio = blackRatio;
    return hr;
}

HRESULT Detection::Detect(TextureView target)
{
    HRESULT hr = S_OK;

    D3D11_TEXTURE2D_DESC target_desc = {};
    target.GetTexture()->GetDesc(&target_desc);

    // Set the shader
    m_context->CSSetShader(m_computeShader.Get(), nullptr, 0);

    // Set the input texture
    ID3D11ShaderResourceView* srv = target.GetSRV();
    m_context->CSSetShaderResources(0, 1, &srv);

    // Set the UAVs for row and column flags
    ID3D11UnorderedAccessView* uavs[] = { m_lumaUAV.Get() };
    m_context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

    // Dispatch compute shader to process rows and columns
    UINT threadGroupCountX = (target_desc.Width + 15) / 16;
    UINT threadGroupCountY = (target_desc.Height + 15) / 16;
    m_context->Dispatch(threadGroupCountX, threadGroupCountY, 1);

    // Clear UAVs after dispatch
    ID3D11UnorderedAccessView* nullUAVs[] = { nullptr, nullptr };
    m_context->CSSetUnorderedAccessViews(0, 2, nullUAVs, nullptr);

    // Unbind
    ID3D11ShaderResourceView* nullSRVs[1] = { nullptr };
    m_context->CSSetShaderResources(0, 1, nullSRVs);
    ID3D11Buffer* nullCBs[1] = { nullptr };
    m_context->CSSetConstantBuffers(0, 1, nullCBs);
    m_context->CSSetShader(nullptr, nullptr, 0);

    m_context->CopyResource(m_lumaStaging.Get(), m_luma.Get());

    D3D11_MAPPED_SUBRESOURCE mappedLuma = {};
    m_context->Map(m_lumaStaging.Get(), 0, D3D11_MAP_READ, 0, &mappedLuma);

    const uint8_t* luma = reinterpret_cast<const uint8_t*>(mappedLuma.pData);
    const UINT pitch = mappedLuma.RowPitch;

    const uint8_t uBlackThreshold = uint8_t(m_blackThreshold * 255.0f);

    UINT minBlackCount = UINT(m_blackRatio * float(m_width));
    m_topBarEnd = -1;
    for (UINT y = 0; y < UINT(m_height); ++y) {
        UINT blackCount = 0;
        const uint8_t* row = luma + y * pitch;
        for (UINT x = 0; x < UINT(m_width); ++x) {
            if (row[x] <= uBlackThreshold) {
                blackCount++;
            }
        }
        if (blackCount < minBlackCount) { m_topBarEnd = int(y) - 1; break; }
    }
    if (m_topBarEnd < 0) m_topBarEnd = 0; // all-black safety

    m_bottomBarStart = int(m_height);
    for (int y = int(m_height) - 1; y >= 0; --y) {
        UINT blackCount = 0;
        const uint8_t* row = luma + y * pitch;
        for (UINT x = 0; x < UINT(m_width); ++x) {
            if (row[x] <= uBlackThreshold) {
                blackCount++;
            }
        }
        if (blackCount < minBlackCount) { m_bottomBarStart = y + 1; break; }
    }

    minBlackCount = UINT(m_blackRatio * float(m_height));
    m_leftBarEnd = -1;
    for (UINT x = 0; x < UINT(m_width); ++x) {
        UINT blackCount = 0;
        for (UINT y = 0; y < UINT(m_height); ++y) {
            const uint8_t* row = luma + y * pitch;
            if (row[x] <= uBlackThreshold) {
                blackCount++;
            }
        }
        if (blackCount < minBlackCount) { m_leftBarEnd = int(x) - 1; break; }
    }
    if (m_leftBarEnd < 0) m_leftBarEnd = 0;

    m_rightBarStart = int(m_width);
    for (int x = int(m_width) - 1; x >= 0; --x) {
        UINT blackCount = 0;
        for (UINT y = 0; y < UINT(m_height); ++y) {
            const uint8_t* row = luma + y * pitch;
            if (row[x] <= uBlackThreshold) {
                blackCount++;
            }
        }
        if (blackCount < minBlackCount) { m_rightBarStart = x + 1; break; }
    }

    //OutputDebugStringA((std::to_string(m_topBarEnd) + "," + std::to_string(m_bottomBarStart) + "," +
    //    std::to_string(m_leftBarEnd) + "," + std::to_string(m_rightBarStart) + "\n").c_str());

    m_context->Unmap(m_lumaStaging.Get(), 0);

    int bar_width = min(m_leftBarEnd, m_width - m_rightBarStart) + 1;
    m_detectWidth = m_width - bar_width * 2;

    int bar_height = min(m_topBarEnd, m_height - m_bottomBarStart) + 1;
    m_detectHeight = m_height - bar_height * 2;

    float detectedAspect = (float)m_detectWidth / (float)m_detectHeight;
    float windowAspect = (float)m_width / (float)m_height;

    // keep either only letterbox or pillarbox
    if (detectedAspect <= windowAspect)
    {
        m_detectHeight = m_height;
    }
    else
    {
        m_detectWidth = m_width;
    }

    if (detectedAspect > 3.0f || detectedAspect < 1.0f)
    {
        // detected aspect is too extreme, ignore
        m_detectWidth = m_width;
        m_detectHeight = m_height;
    }

    // minimum size of the light effects
    if (m_detectWidth > m_width - 16)
    {
        m_detectWidth = m_width;
    }
    if (m_detectHeight > m_height - 16)
    {
        m_detectHeight = m_height;
    }

    return hr;
}

int Detection::GetWidth()
{
    return m_detectWidth;
}   

int Detection::GetHeight()
{
    return m_detectHeight;
}
