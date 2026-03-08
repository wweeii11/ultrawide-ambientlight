#pragma once

#include "windows.h"
#include "d3d11.h"
#include "dxgi1_2.h"
#include "dxgi1_3.h"
#include "wrl/client.h"
#include "SimpleMath.h"
#include <string>
#include <vector>
#include <deque>
#include <numeric>

#include "settings.h"

using namespace Microsoft::WRL;

// shell icon message
#define WM_USER_SHELLICON       (WM_USER + 1)
// win message to toggle UI
// lparam: 0 - use wparam as show/hide, 1 - toggle
// wparam: 0 - hide, 1 - show
#define WM_TOGGLE_CONFIG_WINDOW (WM_USER + 2)
#define WM_WINDOW_ACTIVATED     (WM_USER + 3)

#define RETURN_IF_FAILED(hr) if (FAILED(hr)) return hr;

#define RECT_WIDTH(r) (r.right - r.left)
#define RECT_HEIGHT(r) (r.bottom - r.top)

enum BlackBarPosition
{
    Top,
    Bottom,
    Left,
    Right
};

struct BlackBar
{
    UINT parentWidth;
    UINT parentHeight;
    UINT width;
    UINT height;
    BlackBarPosition position;

    auto operator ==(BlackBar const& other) const
    {
        return parentWidth == other.parentWidth
            && parentHeight == other.parentHeight
            && width == other.width
            && height == other.height
            && position == other.position;
    }

    D3D11_BOX GetBox() const
    {
        UINT clampH = min(height, parentHeight);
        UINT clampW = min(width, parentWidth);
        D3D11_BOX box = {};
        box.front = 0;
        box.back = 1;
        switch (position)
        {
        case Top:
            box.left = 0;
            box.top = 0;
            box.right = parentWidth;
            box.bottom = clampH;
            break;
        case Bottom:
            box.left = 0;
            box.top = parentHeight - clampH;
            box.right = parentWidth;
            box.bottom = parentHeight;
            break;
        case Left:
            box.left = 0;
            box.top = 0;
            box.right = clampW;
            box.bottom = parentHeight;
            break;
        case Right:
            box.left = parentWidth - clampW;
            box.top = 0;
            box.right = parentWidth;
            box.bottom = parentHeight;
            break;
        }
        return box;
    }
};

class TextureView
{
public:
    TextureView()
    {

    }
    void CreateViews(ID3D11Device* device, ID3D11Texture2D* texture, bool createRtv = true, bool createSrv = true, bool createUav = true)
    {
        m_texture = texture;

        if (texture && device)
        {
            if (createSrv)
            {
                D3D11_TEXTURE2D_DESC desc;
                texture->GetDesc(&desc);
                D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                srvDesc.Format = desc.Format;
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Texture2D.MipLevels = desc.MipLevels;
                srvDesc.Texture2D.MostDetailedMip = 0;
                device->CreateShaderResourceView(texture, &srvDesc, &m_srv);
            }

            if (createRtv)
            {
                device->CreateRenderTargetView(texture, nullptr, &m_rtv);
            }

            if (createUav)
            {
                D3D11_TEXTURE2D_DESC desc;
                texture->GetDesc(&desc);
                D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
                uavDesc.Format = desc.Format;
                uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
                uavDesc.Texture2D.MipSlice = 0;
                device->CreateUnorderedAccessView(texture, &uavDesc, &m_uav);
            }
        }
    }
    void Clear()
    {
        m_texture = nullptr;
        m_srv = nullptr;
        m_rtv = nullptr;
    }

    HRESULT RecreateTexture(ID3D11Device* device, DXGI_FORMAT format, UINT width, UINT height)
    {
        HRESULT hr = S_OK;

        if (GetTexture())
        {
            ID3D11Texture2D* texture = GetTexture();
            D3D11_TEXTURE2D_DESC desc;
            texture->GetDesc(&desc);

            if (desc.Format != format || desc.Width != width || desc.Height != height)
            {
                Clear();
            }
        }
        
        if (width == 0 || height == 0)
        {
            return DXGI_ERROR_INVALID_CALL;
        }

        if (!GetTexture())
        {
            ID3D11Texture2D* texture = nullptr;
            D3D11_TEXTURE2D_DESC desc = {};
            desc.Width = width;
            desc.Height = height;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = format;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS;
            desc.CPUAccessFlags = 0;
            desc.MiscFlags = 0;
            hr = device->CreateTexture2D(&desc, nullptr, &texture);
            if (SUCCEEDED(hr))
            {
                CreateViews(device, texture);
                texture->Release();
            }
        }
        return hr;
    }

    ID3D11Texture2D* GetTexture() const
    {
        return m_texture.Get();
    }
    ID3D11ShaderResourceView* GetSRV() const
    {
        return m_srv.Get();
    }
    ID3D11RenderTargetView* GetRTV() const
    {
        return m_rtv.Get();
    }
    ID3D11UnorderedAccessView* GetUAV() const
    {
        return m_uav.Get();
    }

private:
    ComPtr<ID3D11Texture2D> m_texture;
    ComPtr<ID3D11ShaderResourceView> m_srv;
    ComPtr<ID3D11RenderTargetView> m_rtv;
    ComPtr<ID3D11UnorderedAccessView> m_uav;
};

__declspec(align(16))
class PerfTimer {
public:
    PerfTimer() : m_name("Unnamed"), m_maxRecords(10) {
        QueryPerformanceFrequency(&m_frequency);
    }

    PerfTimer(std::string name, size_t maxRecords = 10)
        : m_name(name), m_maxRecords(maxRecords) {
        QueryPerformanceFrequency(&m_frequency);
    }

    void Start() { QueryPerformanceCounter(&m_startTime); }

    void Stop() {
        LARGE_INTEGER endTime;
        QueryPerformanceCounter(&endTime);
        double duration = (double)(endTime.QuadPart - m_startTime.QuadPart) * 1000.0 / m_frequency.QuadPart;

        m_records.push_back(duration);
        if (m_records.size() > m_maxRecords) m_records.pop_front();
    }

    // New: Sends the average and last record to DebugView
    void PrintToDebug() const {
        char buffer[256];
        sprintf_s(buffer, "[%s] Avg: %.3fms | Last: %.3fms\n",
            m_name.c_str(), GetAverage(), GetLast());

        OutputDebugStringA(buffer);
    }

    double GetAverage() const {
        if (m_records.empty()) return 0.0;
        return std::accumulate(m_records.begin(), m_records.end(), 0.0) / m_records.size();
    }

    double GetLast() const { return m_records.empty() ? 0.0 : m_records.back(); }

private:
    std::string m_name;
    LARGE_INTEGER m_frequency;
    LARGE_INTEGER m_startTime;
    std::deque<double> m_records;
    size_t m_maxRecords;
};

struct ScopedPerfTimer {
    PerfTimer& timer;
    ScopedPerfTimer(PerfTimer& t) : timer(t) { timer.Start(); }
    ~ScopedPerfTimer() { timer.Stop(); }
};
