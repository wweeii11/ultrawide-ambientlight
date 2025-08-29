#pragma once

#include "common.h"

// set up windows desktop duplication api
class DesktopCapture
{
public:
    DesktopCapture();
    ~DesktopCapture();
    HRESULT Initialize(ComPtr<ID3D11Device> device);
    HRESULT Capture();
    HRESULT ReleaseFrame();

    ComPtr<ID3D11Texture2D> GetDesktopTexture() { return m_desktopTexture; }
private:
    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGIOutputDuplication> m_duplication;

    ComPtr<ID3D11Texture2D> m_desktopTexture;
};
