#include "capture.h"

#pragma comment(lib, "dxgi.lib")

DesktopCapture::DesktopCapture()
{
}

DesktopCapture::~DesktopCapture()
{
}

HRESULT DesktopCapture::Initialize(ComPtr<ID3D11Device> device)
{
    m_device = device;
    m_device->GetImmediateContext(&m_context);

    ComPtr<IDXGIDevice> dxgiDevice;
    HRESULT hr = m_device.As(&dxgiDevice);
    RETURN_IF_FAILED(hr);

    ComPtr<IDXGIAdapter> dxgiAdapter;
    hr = dxgiDevice->GetParent(__uuidof(IDXGIAdapter), &dxgiAdapter);
    RETURN_IF_FAILED(hr);

    ComPtr<IDXGIOutput> dxgiOutput;
    hr = dxgiAdapter->EnumOutputs(0, &dxgiOutput);
    RETURN_IF_FAILED(hr);

    ComPtr<IDXGIOutput1> dxgiOutput1;
    hr = dxgiOutput.As(&dxgiOutput1);
    RETURN_IF_FAILED(hr);

    hr = dxgiOutput1->DuplicateOutput(m_device.Get(), &m_duplication);
    RETURN_IF_FAILED(hr);

    return hr;
}

HRESULT DesktopCapture::Capture()
{
    if (m_desktopTexture)
    {
        m_desktopTexture = nullptr;
        m_duplication->ReleaseFrame();
    }

    DXGI_OUTDUPL_FRAME_INFO frameInfo;
    ComPtr<IDXGIResource> desktopResource;
    HRESULT hr = m_duplication->AcquireNextFrame(500, &frameInfo, &desktopResource);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT)
    {
        return hr;
    }
    RETURN_IF_FAILED(hr);
    hr = desktopResource.As(&m_desktopTexture);
    RETURN_IF_FAILED(hr);

    return hr;
}
