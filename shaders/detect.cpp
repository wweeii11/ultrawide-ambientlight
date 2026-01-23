#include "detect.h"
#include "d3dcompiler.h"
#include "luma_bin.h"
#include "mask_bin.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

using namespace DirectX;

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
    textureDesc.Format = DXGI_FORMAT_R8_UNORM;
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
    UINT reservedWidth, UINT reservedHeight)
{
    HRESULT hr = S_OK;
    if (m_device != device)
    {
        m_lumaShader = nullptr;
        m_maskShader = nullptr;
        m_width = 0;
        m_height = 0;
    }

    m_device = device;
    m_context = context;

    if (!m_lumaShader || !m_maskShader)
    {
        // create compute shader
        hr = device->CreateComputeShader(g_luma, sizeof(g_luma), nullptr, &m_lumaShader);
        RETURN_IF_FAILED(hr);

        hr = device->CreateComputeShader(g_mask, sizeof(g_mask), nullptr, &m_maskShader);
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

        m_luma.CreateViews(m_device.Get(), lumaTex, false, true, true);
    }

    m_width = width;
    m_height = height;
    m_blackThreshold = blackThreshold;
    m_blackRatio = blackRatio;
    m_symmetricBars = symmetricBars;
    m_topBar = 0;
    m_bottomBar = m_height;
    m_leftBar = 0;
    m_rightBar = m_width;
    m_detectWidth = m_width;
    m_detectHeight = m_height;

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

HRESULT Detection::Detect(ID3D11DeviceContext* context, TextureView target)
{
    HRESULT hr = S_OK;

    D3D11_TEXTURE2D_DESC target_desc = {};
    target.GetTexture()->GetDesc(&target_desc);

    // Set the shader
    context->CSSetShader(m_lumaShader.Get(), nullptr, 0);

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

    const uint8_t* luma = reinterpret_cast<const uint8_t*>(mappedLuma.pData);
    const UINT pitch = mappedLuma.RowPitch;

    const uint8_t uBlackThreshold = uint8_t(m_blackThreshold * 255.0f);

    UINT minBlackCount = UINT(m_blackRatio * float(m_width));
    // minimum detection is 16px
    UINT minBarSize = 16;

    m_topBar = 0;
    for (UINT y = 0; y < UINT(m_height); ++y) {
        UINT blackCount = 0;
        const uint8_t* row = luma + y * pitch;
        for (UINT x = 0; x < UINT(m_width); ++x) {
            if (row[x] <= uBlackThreshold) {
                blackCount++;
            }
        }
        if (blackCount < minBlackCount)
        {
            m_topBar = y;
            break;
        }
    }
    m_topBar = m_topBar < minBarSize ? 0 : m_topBar;

    m_bottomBar = 0;
    for (UINT y = m_height - 1; y >= 0; --y) {
        UINT blackCount = 0;
        const uint8_t* row = luma + y * pitch;
        for (UINT x = 0; x < UINT(m_width); ++x) {
            if (row[x] <= uBlackThreshold) {
                blackCount++;
            }
        }
        if (blackCount < minBlackCount)
        {
            m_bottomBar = (m_height - 1) - y;
            break;
        }
        if (y == 0)
            break;
    }
    m_bottomBar = m_bottomBar < minBarSize ? 0 : m_bottomBar;

    minBlackCount = UINT(m_blackRatio * float(m_height));
    m_leftBar = 0;
    for (UINT x = 0; x < UINT(m_width); ++x) {
        UINT blackCount = 0;
        for (UINT y = 0; y < UINT(m_height); ++y) {
            const uint8_t* row = luma + y * pitch;
            if (row[x] <= uBlackThreshold) {
                blackCount++;
            }
        }
        if (blackCount < minBlackCount)
        {
            m_leftBar = x;
            break;
        }
    }
    m_leftBar = m_leftBar < minBarSize ? 0 : m_leftBar;

    m_rightBar = 0;
    for (UINT x = m_width - 1; x >= 0; --x) {
        UINT blackCount = 0;
        for (UINT y = 0; y < UINT(m_height); ++y) {
            const uint8_t* row = luma + y * pitch;
            if (row[x] <= uBlackThreshold) {
                blackCount++;
            }
        }
        if (blackCount < minBlackCount)
        {
            m_rightBar = (m_width - 1) - x;
            break;
        }
        if (x == 0)
            break;
    }
    m_rightBar = m_rightBar < minBarSize ? 0 : m_rightBar;

    //OutputDebugStringA((std::to_string(m_topBarEnd) + "," + std::to_string(m_bottomBarStart) + "," +
    //    std::to_string(m_leftBarEnd) + "," + std::to_string(m_rightBarStart) + "\n").c_str());

    context->Unmap(m_lumaStaging.Get(), 0);

    if (m_reservedWidth > 0)
    {
        UINT maxBarWidth = (m_width - m_reservedWidth) / 2;
        m_leftBar = min(m_leftBar, maxBarWidth);
        m_rightBar = min(m_rightBar, maxBarWidth);
    }
    if (m_reservedHeight > 0)
    {
        UINT maxBarHeight = (m_height - m_reservedHeight) / 2;
        m_topBar = min(m_topBar, maxBarHeight);
        m_bottomBar = min(m_bottomBar, maxBarHeight);
    }

    if (m_symmetricBars)
    {
        UINT verticalBarSize = min(m_topBar, m_bottomBar);
        m_topBar = verticalBarSize;
        m_bottomBar = verticalBarSize;
        UINT horizontalBarSize = min(m_leftBar, m_rightBar);
        m_leftBar = horizontalBarSize;
        m_rightBar = horizontalBarSize;
    }

    m_detectWidth = m_width - m_leftBar - m_rightBar;
    m_detectHeight = m_height - m_topBar - m_bottomBar;

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

    return hr;
}

HRESULT Detection::RenderMask(ID3D11DeviceContext* context, TextureView target)
{
    HRESULT hr = S_OK;

    D3D11_TEXTURE2D_DESC target_desc = {};
    target.GetTexture()->GetDesc(&target_desc);

    // Set the shader
    context->CSSetShader(m_maskShader.Get(), nullptr, 0);

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

std::vector<D3D11_BOX> Detection::GetFixedBoxes(UINT gameWidth, UINT gameHeight)
{
    std::vector<D3D11_BOX> ret;

    float aspect = (float)gameWidth / (float)gameHeight;
    float windowAspect = (float)m_width / (float)m_height;

    gameHeight = (UINT)m_height;
    gameWidth = (UINT)std::round((float)gameHeight * aspect);

    if (gameWidth > m_width)
    {
        gameWidth = (UINT)m_width;
        gameHeight = (UINT)std::round((float)gameWidth / aspect);
    }
    
    if (aspect > windowAspect)
    {
        UINT barHeight = (m_height - gameHeight) / 2;
        // letterbox
        D3D11_BOX box = {};
        box.front = 0;
        box.back = 1;

        box.left = 0;
        box.top = 0;
        box.right = m_width;
        box.bottom = barHeight;

        ret.push_back(box);
        box.top = m_height - barHeight;
        box.bottom = m_height;
        ret.push_back(box);
    }
    else
    {
        UINT barWidth = (m_width - gameWidth) / 2;
        // pillarbox
        D3D11_BOX box = {};
        box.front = 0;
        box.back = 1;

        box.left = 0;
        box.top = 0;
        box.right = barWidth;
        box.bottom = m_height;
        ret.push_back(box);
        box.left = m_width - barWidth;
        box.right = m_width;
        ret.push_back(box);
    }

    return ret;
}

std::vector<D3D11_BOX> Detection::GetDetectedBoxes()
{
    std::vector<D3D11_BOX> ret;
    if (m_detectWidth == m_width && m_detectHeight == m_height)
    {
        // no bars detected
        return ret;
    }
    else if (m_detectHeight == m_height)
    {
        // pillarbox
        D3D11_BOX box = {};
        box.front = 0;
        box.back = 1;

        box.left = 0;
        box.top = 0;
        box.right = m_leftBar;
        box.bottom = m_height;
        ret.push_back(box);
        box.left = m_width - m_rightBar;
        box.right = m_width;
        ret.push_back(box);
    }
    else if (m_detectWidth == m_width)
    {
        // letterbox
        D3D11_BOX box = {};
        box.front = 0;
        box.back = 1;

        box.left = 0;
        box.top = 0;
        box.right = m_width;
        box.bottom = m_topBar;
        ret.push_back(box);
        box.top = m_height - m_bottomBar;
        box.bottom = m_height;
        ret.push_back(box);
    }

    return ret;
}