#pragma once

#include "common.h"
#include "dxgi1_5.h"
#include "dxgi1_6.h"

// set up windows desktop duplication api
class DesktopCapture
{
public:
    DesktopCapture();
    ~DesktopCapture();
    HRESULT Initialize(ComPtr<ID3D11Device> device, HMONITOR monitor);
    HRESULT Capture();
    HRESULT ReleaseFrame();

    ComPtr<ID3D11Texture2D> GetDesktopTexture() { return m_desktopTexture; }
    DXGI_OUTDUPL_DESC GetDesktopDesc()
    {
        DXGI_OUTDUPL_DESC desc = {};
        if (m_duplication)
        {
            m_duplication->GetDesc(&desc);
        }
        return desc;
    }
    DXGI_OUTPUT_DESC1 GetOutputDesc1() { return m_outputDesc1; }
private:
    ComPtr<ID3D11Device> m_device;
    HMONITOR m_monitor;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGIOutputDuplication> m_duplication;

    ComPtr<ID3D11Texture2D> m_desktopTexture;
    DXGI_OUTPUT_DESC1 m_outputDesc1 = {};
};
