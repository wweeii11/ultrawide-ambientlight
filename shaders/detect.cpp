#include "detect.h"
#include "detectCpu.h"
#include "d3dcompiler.h"
#include "luma_mainSDR_bin.h"
#include "luma_mainSCRGB_bin.h"
#include "luma_mainHDR10_bin.h"
#include "mask_main_bin.h"
#include "mask_mainUNorm_bin.h"
#include "luma_CSRowAnalysis_bin.h"
#include "luma_CSColAnalysis_bin.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

using namespace DirectX;

#define SDR_LUMA_THRESHOLD 0.01f
#define HDR10_LUMA_THRESHOLD 0.0001f

__declspec(align(16))
struct DetectionParams {
    UINT  Width;
    UINT  Height;
    float BlackThreshold;
    float BlackRatio;
    float VarianceThreshold;
    //float Padding[3];
};

Detection::Detection()
    : m_width(0), m_height(0),
    m_blackThreshold(0.0f), m_blackRatio(0.0f), m_symmetricBars(false),
    m_topBar(0), m_bottomBar(0), m_leftBar(0), m_rightBar(0),
    m_reservedWidth(0), m_reservedHeight(0),
    m_colorSpace(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709)
{
}

Detection::~Detection()
{
}

HRESULT Detection::Initialize(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context,
    UINT width, UINT height,
    float blackThreshold, float blackRatio, bool symmetricBars,
    UINT reservedWidth, UINT reservedHeight,
    DXGI_COLOR_SPACE_TYPE colorSpace)
{
    HRESULT hr = S_OK;
    if (m_device != device)
    {
        m_lumaShader = nullptr;
        m_lumaHDR10Shader = nullptr;
        m_lumaSCRGBShader = nullptr;
        m_lumaMaskShader = nullptr;
        m_width = 0;
        m_height = 0;
    }

    m_device = device;
    m_context = context;

    if (!m_lumaShader || !m_lumaMaskShader || !m_lumaHDR10Shader || !m_lumaSCRGBShader || !m_lumaMaskShaderUNorm)
    {
        // create compute shader
        hr = device->CreateComputeShader(g_luma_mainSDR, sizeof(g_luma_mainSDR), nullptr, &m_lumaShader);
        RETURN_IF_FAILED(hr);

        hr = device->CreateComputeShader(g_luma_mainHDR10, sizeof(g_luma_mainHDR10), nullptr, &m_lumaHDR10Shader);
        RETURN_IF_FAILED(hr);

        hr = device->CreateComputeShader(g_luma_mainSCRGB, sizeof(g_luma_mainSCRGB), nullptr, &m_lumaSCRGBShader);
        RETURN_IF_FAILED(hr);

        hr = device->CreateComputeShader(g_mask_main, sizeof(g_mask_main), nullptr, &m_lumaMaskShader);
        RETURN_IF_FAILED(hr);

        hr = device->CreateComputeShader(g_mask_mainUNorm, sizeof(g_mask_mainUNorm), nullptr, &m_lumaMaskShaderUNorm);
        RETURN_IF_FAILED(hr);

        hr = device->CreateComputeShader(g_luma_CSRowAnalysis, sizeof(g_luma_CSRowAnalysis), nullptr, &m_rowAnalysisShader);
        RETURN_IF_FAILED(hr);

        hr = device->CreateComputeShader(g_luma_CSColAnalysis, sizeof(g_luma_CSColAnalysis), nullptr, &m_colAnalysisShader);
        RETURN_IF_FAILED(hr);
    }


    if (m_width != width || m_height != height)
    {
        m_width = width;
        m_height = height;
        m_topBar = 0;
        m_bottomBar = m_height;
        m_leftBar = 0;
        m_rightBar = m_width;

        CreateBuffers();
    }

    m_blackRatio = blackRatio;
    m_symmetricBars = symmetricBars;

    m_colorSpace = colorSpace;

    // maximum threshold for black detection
    // blackThreshold is user setting between 0.0 and 1.0
    m_blackThreshold = blackThreshold * SDR_LUMA_THRESHOLD;
    m_blackVariance = 1e-6f;

    // Adjust threshold for HDR10 PQ content
    switch (m_colorSpace)
    {
    case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
        m_blackThreshold = blackThreshold * HDR10_LUMA_THRESHOLD;
        m_blackVariance = 1e-10f;
        break;
    }

    DetectionParams params = {};
    params.Width = m_width;
    params.Height = m_height;
    params.BlackThreshold = m_blackThreshold;
    params.BlackRatio = m_blackRatio;
    params.VarianceThreshold = m_blackVariance;

    context->UpdateSubresource(m_constants.Get(), 0, nullptr, &params, 0, 0);

    m_reservedWidth = reservedWidth;
    m_reservedHeight = reservedHeight;
    if (reservedWidth > 0 && reservedHeight > 0)
    {
        float reservedAspect = (float)reservedWidth / (float)reservedHeight;
        float windowAspect = (float)m_width / (float)m_height;

        m_reservedHeight = (UINT)m_height;
        m_reservedWidth = (UINT)std::round((float)m_reservedHeight * reservedAspect);

        if (m_reservedWidth > m_width)
        {
            m_reservedWidth = (UINT)m_width;
            m_reservedHeight = (UINT)std::round((float)m_reservedWidth / reservedAspect);
        }
    }

    return hr;
}

HRESULT Detection::CreateBuffers()
{
    HRESULT hr = S_OK;

    ComPtr<ID3D11Texture2D> tex = nullptr;

    D3D11_TEXTURE2D_DESC textureDesc;
    ZeroMemory(&textureDesc, sizeof(textureDesc));

    textureDesc.Width = m_width;
    textureDesc.Height = m_height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R32_FLOAT;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = 0;

    hr = m_device->CreateTexture2D(&textureDesc, nullptr, &tex);
    RETURN_IF_FAILED(hr);

    m_luma.CreateViews(m_device.Get(), tex.Get(), false, true, true);

    // Staging texture
    textureDesc.Usage = D3D11_USAGE_STAGING;
    textureDesc.BindFlags = 0;
    textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    m_device->CreateTexture2D(&textureDesc, nullptr, &tex);
    RETURN_IF_FAILED(hr);

    m_lumaStaging = tex;

    // --- 1. CONSTANT BUFFER SETUP ---
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(DetectionParams); // Ensure this struct is padded to 32 bytes
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = 0;
    cbDesc.MiscFlags = 0;
    cbDesc.StructureByteStride = 0;

    hr = m_device->CreateBuffer(&cbDesc, nullptr, &m_constants);
    RETURN_IF_FAILED(hr);

    // --- 2. ROW ANALYSIS BUFFERS (Tracking vertical lines -> bounded by HEIGHT) ---
    D3D11_BUFFER_DESC bufDesc = {};
    bufDesc.ByteWidth = m_height * sizeof(UINT);
    bufDesc.Usage = D3D11_USAGE_DEFAULT;
    bufDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    bufDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bufDesc.StructureByteStride = sizeof(UINT);

    hr = m_device->CreateBuffer(&bufDesc, nullptr, &m_rowResults);
    RETURN_IF_FAILED(hr);

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN; // Correct for unstructured/structured arrays
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = m_height; // Must equal height for row indexing
    uavDesc.Buffer.Flags = 0;

    hr = m_device->CreateUnorderedAccessView(m_rowResults.Get(), &uavDesc, &m_rowResultsUAV);
    RETURN_IF_FAILED(hr);

    // Row Staging Buffer for CPU readback
    D3D11_BUFFER_DESC stageDesc = {};
    stageDesc.ByteWidth = m_height * sizeof(UINT);
    stageDesc.Usage = D3D11_USAGE_STAGING;
    stageDesc.BindFlags = 0; // CRITICAL: Staging buffers cannot bind to the pipeline slots
    stageDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stageDesc.MiscFlags = 0;
    stageDesc.StructureByteStride = 0;

    hr = m_device->CreateBuffer(&stageDesc, nullptr, &m_rowStaging);
    RETURN_IF_FAILED(hr);

    // --- 3. COLUMN ANALYSIS BUFFERS (Tracking horizontal lines -> bounded by WIDTH) ---
    bufDesc.ByteWidth = m_width * sizeof(UINT);
    hr = m_device->CreateBuffer(&bufDesc, nullptr, &m_colResults);
    RETURN_IF_FAILED(hr);

    uavDesc.Buffer.NumElements = m_width; // Must equal width for column indexing
    hr = m_device->CreateUnorderedAccessView(m_colResults.Get(), &uavDesc, &m_colResultsUAV);
    RETURN_IF_FAILED(hr);

    // Column Staging Buffer for CPU readback
    stageDesc.ByteWidth = m_width * sizeof(UINT);
    hr = m_device->CreateBuffer(&stageDesc, nullptr, &m_colStaging);
    RETURN_IF_FAILED(hr);

    return hr;
}

HRESULT Detection::DispatchRowColAnalysis(ID3D11DeviceContext* context)
{
    HRESULT hr = S_OK;

    context->CSSetConstantBuffers(0, 1, m_constants.GetAddressOf());

    // Run Row Analysis Shader
    ID3D11ShaderResourceView* srv = m_luma.GetSRV();
    ID3D11UnorderedAccessView* uavs = m_rowResultsUAV.Get();

    context->CSSetShader(m_rowAnalysisShader.Get(), nullptr, 0);
    context->CSSetShaderResources(1, 1, &srv);
    context->CSSetUnorderedAccessViews(1, 1, &uavs, nullptr);
    context->Dispatch((m_height + 63) / 64, 1, 1);

    ID3D11UnorderedAccessView* nullUAV = nullptr;
    context->CSSetUnorderedAccessViews(1, 1, &nullUAV, nullptr);

    // Run Column Analysis Shader
    uavs = m_colResultsUAV.Get();

    context->CSSetShader(m_colAnalysisShader.Get(), nullptr, 0);
    context->CSSetShaderResources(1, 1, &srv);
    context->CSSetUnorderedAccessViews(1, 1, &uavs, nullptr);
    context->Dispatch((m_width + 63) / 64, 1, 1);

    // clean up
    context->CSSetUnorderedAccessViews(1, 1, &nullUAV, nullptr);
    ID3D11ShaderResourceView* nullSRV = nullptr;
    context->CSSetShaderResources(1, 1, &nullSRV);
    ID3D11Buffer* nullBuffer = nullptr;
    context->CSSetConstantBuffers(1, 1, &nullBuffer);

    context->CopyResource(m_rowStaging.Get(), m_rowResults.Get());
    context->CopyResource(m_colStaging.Get(), m_colResults.Get());

    return hr;
}

HRESULT Detection::FetchRowResults(ID3D11DeviceContext* pContext)
{
    HRESULT hr = S_OK;
    D3D11_MAPPED_SUBRESOURCE mappedFlags;
    hr = pContext->Map(m_rowStaging.Get(), 0, D3D11_MAP_READ, 0, &mappedFlags);
    RETURN_IF_FAILED(hr);

    const UINT* rowActiveFlags = reinterpret_cast<const UINT*>(mappedFlags.pData);
    const int vCenter = m_height / 2;

    m_topBar = FindBarSizeCenterOutWithFlags(rowActiveFlags, vCenter, -1, 0);
    m_bottomBar = FindBarSizeCenterOutWithFlags(rowActiveFlags, vCenter, 1, m_height - 1);

    pContext->Unmap(m_rowStaging.Get(), 0);
    return hr;
}

HRESULT Detection::FetchColResults(ID3D11DeviceContext* pContext)
{
    HRESULT hr = S_OK;
    D3D11_MAPPED_SUBRESOURCE mappedFlags;
    hr = pContext->Map(m_colStaging.Get(), 0, D3D11_MAP_READ, 0, &mappedFlags);
    RETURN_IF_FAILED(hr);

    const UINT* colActiveFlags = reinterpret_cast<const UINT*>(mappedFlags.pData);
    const int hCenter = m_width / 2;

    m_leftBar = FindBarSizeCenterOutWithFlags(colActiveFlags, hCenter, -1, 0);
    m_rightBar = FindBarSizeCenterOutWithFlags(colActiveFlags, hCenter, 1, m_width - 1);

    pContext->Unmap(m_colStaging.Get(), 0);
    return hr;
}

HRESULT Detection::Detect(ID3D11DeviceContext* context, TextureView target)
{
    HRESULT hr = S_OK;

    hr = DispatchLuma(context, target);
    RETURN_IF_FAILED(hr);

    hr = DispatchRowColAnalysis(context);
    RETURN_IF_FAILED(hr);

    hr = FetchRowResults(context);
    RETURN_IF_FAILED(hr);
    hr = FetchColResults(context);
    RETURN_IF_FAILED(hr);

#ifdef CPU_DETECTION
    // minimum detection is 16px
    UINT minBarSize = 16;

    context->CopyResource(m_lumaStaging.Get(), m_luma.GetTexture());

    D3D11_MAPPED_SUBRESOURCE mappedLuma = {};
    context->Map(m_lumaStaging.Get(), 0, D3D11_MAP_READ, 0, &mappedLuma);

    const float* luma = reinterpret_cast<const float*>(mappedLuma.pData);
    const UINT pitch = mappedLuma.RowPitch / sizeof(float);

    // inside out detection
    const int hCenter = m_width / 2;
    const int vCenter = m_height / 2;

    m_topBar = FindBarSizeCenterOut(luma, pitch, m_width, m_height,
        true, vCenter, -1, 0,
        blackThreshold, m_blackRatio, blackVariance, minBarSize);

    m_bottomBar = FindBarSizeCenterOut(luma, pitch, m_width, m_height,
        true, vCenter, 1, m_height - 1,
        blackThreshold, m_blackRatio, blackVariance, minBarSize);

    m_leftBar = FindBarSizeCenterOut(luma, pitch, m_width, m_height,
        false, hCenter, -1, 0,
        blackThreshold, m_blackRatio, blackVariance, minBarSize);

    m_rightBar = FindBarSizeCenterOut(luma, pitch, m_width, m_height,
        false, hCenter, 1, m_width - 1,
        blackThreshold, m_blackRatio, blackVariance, minBarSize);

    context->Unmap(m_lumaStaging.Get(), 0);
#endif

    //OutputDebugStringA((std::to_string(m_topBarEnd) + "," + std::to_string(m_bottomBarStart) + "," +
    //    std::to_string(m_leftBarEnd) + "," + std::to_string(m_rightBarStart) + "\n").c_str());

    if (m_symmetricBars)
    {
        UINT verticalBarSize = min(m_topBar, m_bottomBar);
        m_topBar = verticalBarSize;
        m_bottomBar = verticalBarSize;
        UINT horizontalBarSize = min(m_leftBar, m_rightBar);
        m_leftBar = horizontalBarSize;
        m_rightBar = horizontalBarSize;
    }

    // sanity check
    if (m_leftBar >= m_width / 2 - 16)
        m_leftBar = 0;
    if (m_rightBar >= m_width / 2 - 16)
        m_rightBar = 0;
    if (m_topBar >= m_height / 2 - 16)
        m_topBar = 0;
    if (m_bottomBar >= m_height / 2 - 16)
        m_bottomBar = 0;

    return hr;
}

HRESULT Detection::DispatchLuma(ID3D11DeviceContext* context, TextureView target)
{
    HRESULT hr = S_OK;

    D3D11_TEXTURE2D_DESC target_desc = {};
    if (!target.GetTexture())
        return E_INVALIDARG;

    target.GetTexture()->GetDesc(&target_desc);

    // Set the shader
    switch (target_desc.Format)
    {
    case DXGI_FORMAT_R10G10B10A2_UNORM:
        if (m_colorSpace == DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709)
        {
            context->CSSetShader(m_lumaShader.Get(), nullptr, 0);
        }
        else
        {
            context->CSSetShader(m_lumaHDR10Shader.Get(), nullptr, 0);
        }
        break;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
        if (m_colorSpace == DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709)
        {
            context->CSSetShader(m_lumaSCRGBShader.Get(), nullptr, 0);
        }
        else
        {
            context->CSSetShader(m_lumaHDR10Shader.Get(), nullptr, 0);
        }
        break;
    default:
        context->CSSetShader(m_lumaShader.Get(), nullptr, 0);
        break;
    }

    // Set the input texture
    ID3D11ShaderResourceView* srv = target.GetSRV();
    context->CSSetShaderResources(0, 1, &srv);

    // Set output luma UAV
    ID3D11UnorderedAccessView* uavs[] = { m_luma.GetUAV() };
    context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

    // Dispatch for entire input texture
    UINT threadGroupCountX = (target_desc.Width + 15) / 16;
    UINT threadGroupCountY = (target_desc.Height + 15) / 16;
    context->Dispatch(threadGroupCountX, threadGroupCountY, 1);

    // Clear UAVs after dispatch
    ID3D11UnorderedAccessView* nullUAVs[] = { nullptr };
    context->CSSetUnorderedAccessViews(0, 1, nullUAVs, nullptr);

    // Unbind
    ID3D11ShaderResourceView* nullSRVs[1] = { nullptr };
    context->CSSetShaderResources(0, 1, nullSRVs);
    ID3D11Buffer* nullCBs[1] = { nullptr };
    context->CSSetConstantBuffers(0, 1, nullCBs);
    context->CSSetShader(nullptr, nullptr, 0);

    return hr;
}

HRESULT Detection::RenderLumaMask(ID3D11DeviceContext* context, TextureView target)
{
    HRESULT hr = S_OK;

    D3D11_TEXTURE2D_DESC target_desc = {};
    target.GetTexture()->GetDesc(&target_desc);

    // Set the shader
    if (target_desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT)
    {
        context->CSSetShader(m_lumaMaskShader.Get(), nullptr, 0);
    }
    else
    {
        context->CSSetShader(m_lumaMaskShaderUNorm.Get(), nullptr, 0);
    }

    // Set the input texture
    ID3D11ShaderResourceView* srv = m_luma.GetSRV();
    context->CSSetShaderResources(0, 1, &srv);

    ID3D11UnorderedAccessView* uav = target.GetUAV();
    context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);

    DetectionParams params = {};
    params.Width = m_width;
    params.Height = m_height;
    params.BlackThreshold = m_blackThreshold;
    params.BlackRatio = m_blackRatio;
    params.VarianceThreshold = m_blackVariance;

    context->CSSetConstantBuffers(0, 1, m_constants.GetAddressOf());

    context->Dispatch(
        (target_desc.Width + 15) / 16,
        (target_desc.Height + 15) / 16,
        1);

    uav = nullptr;
    context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
    srv = nullptr;
    context->CSSetShaderResources(0, 1, &srv);
    ID3D11Buffer* nullBuffer = nullptr;
    context->CSSetConstantBuffers(1, 1, &nullBuffer);

    return hr;
}


std::vector<BlackBar> Detection::GetFixedBars(UINT windowWidth, UINT windowHeight, UINT gameWidth, UINT gameHeight)
{
    std::vector<BlackBar> ret;

    float aspect = (float)gameWidth / (float)gameHeight;
    float windowAspect = (float)windowWidth / (float)windowHeight;

    gameHeight = (UINT)windowHeight;
    gameWidth = (UINT)std::round((float)gameHeight * aspect);

    if (gameWidth > windowWidth)
    {
        gameWidth = (UINT)windowWidth;
        gameHeight = (UINT)std::round((float)gameWidth / aspect);
    }

    if (aspect > windowAspect)
    {
        UINT barHeight = (windowHeight - gameHeight) / 2;
        // letterbox
        BlackBar topBar = {};
        topBar.parentWidth = windowWidth;
        topBar.parentHeight = windowHeight;
        topBar.width = windowWidth;
        topBar.height = barHeight;
        topBar.position = Top;

        BlackBar bottomBar = topBar;
        bottomBar.position = Bottom;

        ret.push_back(topBar);
        ret.push_back(bottomBar);

    }
    else
    {
        UINT barWidth = (windowWidth - gameWidth) / 2;
        // pillarbox
        BlackBar leftBar = {};
        leftBar.parentWidth = windowWidth;
        leftBar.parentHeight = windowHeight;
        leftBar.width = barWidth;
        leftBar.height = windowHeight;
        leftBar.position = Left;

        BlackBar rightBar = leftBar;
        rightBar.position = Right;

        ret.push_back(leftBar);
        ret.push_back(rightBar);
    }

    return ret;
}

std::vector<BlackBar> Detection::GetDetectedBars()
{
    std::vector<BlackBar> ret;

    if (m_device == nullptr)
        return ret;

    UINT left = m_leftBar;
    UINT right = m_rightBar;
    UINT top = m_topBar;
    UINT bottom = m_bottomBar;

    UINT leftRight = left + right;
    UINT topBottom = top + bottom;

    if (leftRight > topBottom)
    {
        // pillarbox
        // apply reserved area only for primary bars
        if (m_reservedWidth > 0)
        {
            UINT maxBarWidth = (m_width - m_reservedWidth) / 2;
            left = min(left, maxBarWidth);
            right = min(right, maxBarWidth);
        }

        BlackBar leftBar = {};
        leftBar.parentWidth = m_width;
        leftBar.parentHeight = m_height;
        leftBar.width = left;
        leftBar.height = m_height;
        leftBar.position = Left;

        BlackBar rightBar = leftBar;
        rightBar.width = right;
        rightBar.position = Right;

        ret.push_back(leftBar);
        ret.push_back(rightBar);

        // add secondary bars
        if (top > 0 && bottom > 0)
        {
            BlackBar topBar = {};
            topBar.parentWidth = m_width;
            topBar.parentHeight = m_height;
            topBar.width = m_width;
            topBar.height = top;
            topBar.position = Top;
            BlackBar bottomBar = topBar;
            bottomBar.height = bottom;
            bottomBar.position = Bottom;
            ret.push_back(topBar);
            ret.push_back(bottomBar);
        }
    }
    else
    {
        // letterbox
        if (m_reservedHeight > 0)
        {
            UINT maxBarHeight = (m_height - m_reservedHeight) / 2;
            top = min(top, maxBarHeight);
            bottom = min(bottom, maxBarHeight);
        }

        BlackBar topBar = {};
        topBar.parentWidth = m_width;
        topBar.parentHeight = m_height;
        topBar.width = m_width;
        topBar.height = top;
        topBar.position = Top;

        BlackBar bottomBar = topBar;
        bottomBar.height = bottom;
        bottomBar.position = Bottom;

        ret.push_back(topBar);
        ret.push_back(bottomBar);

        if (left > 0 && right > 0)
        {
            BlackBar leftBar = {};
            leftBar.parentWidth = m_width;
            leftBar.parentHeight = m_height;
            leftBar.width = left;
            leftBar.height = m_height;
            leftBar.position = Left;
            BlackBar rightBar = leftBar;
            rightBar.width = right;
            rightBar.position = Right;
            ret.push_back(leftBar);
            ret.push_back(rightBar);
        }
    }

    return ret;
}
