#include "detect.h"
#include "d3dcompiler.h"
#include "luma_mainSDR_bin.h"
#include "luma_mainSCRGB_bin.h"
#include "luma_mainHDR10_bin.h"
#include "mask_main_bin.h"
#include "mask_mainUNorm_bin.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

using namespace DirectX;

#define SDR_LUMA_THRESHOLD 0.01f
#define HDR10_LUMA_THRESHOLD 0.0001f

Detection::Detection()
{
}

Detection::~Detection()
{
}

static void CreateLumaMaskAndStaging(
    ID3D11Device* dev,
    UINT width,
    UINT height,
    ID3D11Texture2D** gpuTexOut,           // UAV-capable DEFAULT buffer
    ID3D11Texture2D** stagingOut)          // STAGING copy for CPU read
{
    D3D11_TEXTURE2D_DESC textureDesc;
    ZeroMemory(&textureDesc, sizeof(textureDesc));

    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R32_FLOAT;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = 0;
    dev->CreateTexture2D(&textureDesc, nullptr, gpuTexOut);

    // Staging texture
    textureDesc.Usage = D3D11_USAGE_STAGING;
    textureDesc.BindFlags = 0;
    textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    dev->CreateTexture2D(&textureDesc, nullptr, stagingOut);
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

    }


    if (m_width != width || m_height != height)
    {
        ID3D11Texture2D* lumaTex = nullptr;
        CreateLumaMaskAndStaging(
            device.Get(),
            width,
            height,
            &lumaTex,
            &m_lumaStaging);

        if (lumaTex)
        {
            m_luma.CreateViews(m_device.Get(), lumaTex, false, true, true);
            lumaTex->Release();
        }
    }

    if (m_width != width || m_height != height)
    {
        m_topBar = 0;
        m_bottomBar = m_height;
        m_leftBar = 0;
        m_rightBar = m_width;
    }
    m_width = width;
    m_height = height;

    m_blackThreshold = blackThreshold;
    m_blackRatio = blackRatio;
    m_symmetricBars = symmetricBars;

    m_colorSpace = colorSpace;

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

inline bool isLineMostlyBlack(const float* data, UINT length, UINT stride, float blackThreshold, float blackRatio, float varianceThreshold)
{
    int darkPixelCount = 0;
    float sum = 0.0f;
    float sumSq = 0.0f;

    for (UINT i = 0; i < length; ++i)
    {
        float pixel = data[i * stride];
        if (pixel <= blackThreshold)
        {
            ++darkPixelCount;
            sum += pixel;
            sumSq += pixel * pixel;
        }
    }

    if (darkPixelCount == 0)
        return false;

    float darkRatio = (float)darkPixelCount / (float)length;
    if (darkRatio < blackRatio)
        return false;

    // Variance of the dark pixels only: Var = E[x^2] - E[x]^2
    float n = (float)darkPixelCount;
    float mean = sum / n;
    float variance = (sumSq / n) - (mean * mean);

    // True black (bars/chrome) has near-zero variance — all pixels clump around 0
    // Dark HDR content has measurable variance even if all pixels are below threshold
    return (variance <= varianceThreshold);
}

// The Core Center-Out Algorithm - Returns the size of the bar in pixels
UINT FindBarSizeCenterOut(const float* luma, UINT pitch, int width, int height,
    bool isVerticalScan, int startIdx, int step, int edgeIdx,
    float blackThreshold, float blackRatio, float blackVariance, int minBarSize)
{
    // Tracks the furthest line out that we are SURE belongs to the continuous content
    int lastKnownContentLine = startIdx;

    int lineLength = isVerticalScan ? width : height;
    UINT stride = isVerticalScan ? 1 : pitch;

    // Scan from the center out to the edge
    for (int i = startIdx; (step > 0) ? (i <= edgeIdx) : (i >= edgeIdx); i += step)
    {
        const float* linePtr = isVerticalScan ? (luma + i * pitch) : (luma + i);
        
        if (!isLineMostlyBlack(linePtr, lineLength, stride, blackThreshold, blackRatio, blackVariance))
        {
            int blockStart = i;
            int blockLength = 0;

            // 1. Measure the continuous block of bright lines ahead of us
            while ((step > 0 ? (i <= edgeIdx) : (i >= edgeIdx)) &&
                !isLineMostlyBlack(isVerticalScan ? (luma + i * pitch) : (luma + i),
                    lineLength, stride, blackThreshold, blackRatio, blackVariance))
            {
                blockLength++;
                i += step;
            }

            // 2. Dynamically calculate the verified content size so far (one-sided from center)
            int currentContentHalfSize = std::abs(lastKnownContentLine - startIdx);

            // Safety fallback: if we are right at the center and size is 0, provide a tiny baseline 
            // floor so we don't multiply by zero on the first few iterations.
            if (currentContentHalfSize < 4) { currentContentHalfSize = 4; }

            // 3. Define our adaptive thresholds relative to the current content footprint
            // Subtitles/Progress bars rarely exceed 5% to 8% of a video's half-dimension.
            int adaptiveUiThicknessMax = static_cast<int>(currentContentHalfSize * 0.08f);
            int adaptiveGapMax = static_cast<int>(currentContentHalfSize * 0.05f);

            int blackGapLeftBehind = std::abs(blockStart - lastKnownContentLine);

            // 4. Proportional Evaluation
            if (blockLength > adaptiveUiThicknessMax || blackGapLeftBehind <= adaptiveGapMax)
            {
                // It's either a massive block of light (content after a dark scene)
                // OR it's a thin line sitting almost directly flush against the content frame.
                // Extend the content boundaries to include this block.
                lastKnownContentLine = i - step;
            }
            else
            {
                // It's a thin bright block isolated by a substantial black gap.
                // Structurally, this behaves exactly like a UI element. Ignore it.
            }

            // Adjust loop index back by one step because the while loop went one line too far
            i -= step;
        }
    }

    // Convert last known content line into the final black bar size
    int barSize = 0;
    if (step < 0)
    {
        barSize = lastKnownContentLine;
    }
    else
    {
        int maxEdge = isVerticalScan ? height : width;
        barSize = maxEdge - 1 - lastKnownContentLine;
    }

    return (barSize < minBarSize) ? 0 : static_cast<UINT>(barSize);
}

HRESULT Detection::Detect(ID3D11DeviceContext* context, TextureView target)
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

    // Set the UAVs for row and column flags
    ID3D11UnorderedAccessView* uavs[] = { m_luma.GetUAV() };
    context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

    // Dispatch compute shader to process rows and columns
    UINT threadGroupCountX = (target_desc.Width + 15) / 16;
    UINT threadGroupCountY = (target_desc.Height + 15) / 16;
    context->Dispatch(threadGroupCountX, threadGroupCountY, 1);

    // Clear UAVs after dispatch
    ID3D11UnorderedAccessView* nullUAVs[] = { nullptr, nullptr };
    context->CSSetUnorderedAccessViews(0, 2, nullUAVs, nullptr);

    // Unbind
    ID3D11ShaderResourceView* nullSRVs[1] = { nullptr };
    context->CSSetShaderResources(0, 1, nullSRVs);
    ID3D11Buffer* nullCBs[1] = { nullptr };
    context->CSSetConstantBuffers(0, 1, nullCBs);
    context->CSSetShader(nullptr, nullptr, 0);

    context->CopyResource(m_lumaStaging.Get(), m_luma.GetTexture());

    D3D11_MAPPED_SUBRESOURCE mappedLuma = {};
    context->Map(m_lumaStaging.Get(), 0, D3D11_MAP_READ, 0, &mappedLuma);

    const float* luma = reinterpret_cast<const float*>(mappedLuma.pData);
    const UINT pitch = mappedLuma.RowPitch / sizeof(float);

    // minimum detection is 16px
    UINT minBarSize = 16;

    // maximum threshold for black detection
    float maxBlackThreshold = SDR_LUMA_THRESHOLD;
    float blackVariance = 1e-6f;
    // Adjust threshold for HDR10 PQ content
    switch (m_colorSpace)
    {
    case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
        maxBlackThreshold = HDR10_LUMA_THRESHOLD;
        blackVariance = 1e-10f;
        break;
    }

    // m_blackThreshold is user setting between 0.0 and 1.0
    float blackThreshold = m_blackThreshold * maxBlackThreshold;

#ifndef OLD_DETECTION
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

#else
    m_topBar = 0;
    for (UINT y = 0; y < UINT(m_height); ++y) {
        const float* row = luma + y * pitch;
        if (!isLineMostlyBlack(row, m_width, 1, blackThreshold, m_blackRatio, blackVariance))
        {
            m_topBar = y;
            break;
        }
    }
    m_topBar = m_topBar < minBarSize ? 0 : m_topBar;

    m_bottomBar = 0;
    for (UINT y = m_height - 1; y >= 0; --y) {
        const float* row = luma + y * pitch;
        if (!isLineMostlyBlack(row, m_width, 1, blackThreshold, m_blackRatio, blackVariance))
        {
            m_bottomBar = (m_height - 1) - y;
            break;
        }
        if (y == 0)
            break;
    }
    m_bottomBar = m_bottomBar < minBarSize ? 0 : m_bottomBar;

    m_leftBar = 0;
    for (UINT x = 0; x < UINT(m_width); ++x) {
        const float* start = luma + x;
        if (!isLineMostlyBlack(start, m_height, m_width, blackThreshold, m_blackRatio, blackVariance))
        {
            m_leftBar = x;
            break;
        }
    }
    m_leftBar = m_leftBar < minBarSize ? 0 : m_leftBar;

    m_rightBar = 0;
    for (UINT x = m_width - 1; x >= 0; --x) {
        const float* start = luma + x;
        if (!isLineMostlyBlack(start, m_height, m_width, blackThreshold, m_blackRatio, blackVariance))
        {
            m_rightBar = (m_width - 1) - x;
            break;
        }
        if (x == 0)
            break;
    }
    m_rightBar = m_rightBar < minBarSize ? 0 : m_rightBar;
#endif

    //OutputDebugStringA((std::to_string(m_topBarEnd) + "," + std::to_string(m_bottomBarStart) + "," +
    //    std::to_string(m_leftBarEnd) + "," + std::to_string(m_rightBarStart) + "\n").c_str());

    context->Unmap(m_lumaStaging.Get(), 0);

    if (m_symmetricBars)
    {
        UINT verticalBarSize = min(m_topBar, m_bottomBar);
        m_topBar = verticalBarSize;
        m_bottomBar = verticalBarSize;
        UINT horizontalBarSize = min(m_leftBar, m_rightBar);
        m_leftBar = horizontalBarSize;
        m_rightBar = horizontalBarSize;
    }

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

    context->Dispatch(
        (target_desc.Width + 15) / 16,
        (target_desc.Height + 15) / 16,
        1);

    uav = nullptr;
    context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
    srv = nullptr;
    context->CSSetShaderResources(0, 1, &srv);

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
