#include "copy.h"
#include "d3dcompiler.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

using namespace DirectX;

const char* COPY_VS = R"(

struct VS_INPUT
{
    float3 Pos : POSITION;
    float2 Tex : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;
    output.Pos = float4(input.Pos, 1.0f);
    output.Tex = input.Tex;
    return output;
}

)";

const char* COPY_PS = R"(

Texture2D txSource : register(t0);
SamplerState samLinear : register(s0);

float4 PS(float4 Pos : SV_POSITION, float2 Tex : TEXCOORD0) : SV_Target
{
    // Apply bilinear sampling when reading from the source texture
    return txSource.Sample(samLinear, Tex);
}

)";

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT2 texCoord;
};

Copy::Copy()
{
}

Copy::~Copy()
{
}

HRESULT Copy::Initialize(ComPtr<ID3D11Device> device)
{
    HRESULT hr = S_OK;
	m_device = device;
	m_device->GetImmediateContext(&m_context);

    ID3DBlob* errorBlob = nullptr;

	// Compile and create vertex shader
	ID3DBlob* vsBlob = nullptr;
    hr = D3DCompile(COPY_VS, strlen(COPY_VS), "VS", nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, &errorBlob);
	RETURN_IF_FAILED(hr);
	hr = m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vertexShader);
    RETURN_IF_FAILED(hr);

    // Create input layout
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT numElements = ARRAYSIZE(layout);

    hr = device->CreateInputLayout(layout, numElements, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_vertexLayout);
    vsBlob->Release();
    RETURN_IF_FAILED(hr);
    

    // Compile and create pixel shader
    ID3DBlob* psBlob = nullptr;
    hr = D3DCompile(COPY_PS, strlen(COPY_PS), "PS", nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, &errorBlob);
    RETURN_IF_FAILED(hr);

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader);
    psBlob->Release();
    RETURN_IF_FAILED(hr);
    
    // Define a simple quad with texture coordinates
    Vertex quadVertices[] =
    {
        { XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3(-1.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(1.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(1.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) },
    };

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.ByteWidth = sizeof(quadVertices);
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = quadVertices;

    hr = device->CreateBuffer(&bufferDesc, &initData, &m_vertexBuffer);
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
    
	return hr;
}

HRESULT Copy::Apply(TextureView target, TextureView source)
{
    HRESULT hr = S_OK;

    D3D11_TEXTURE2D_DESC target_desc = {};
	target.GetTexture()->GetDesc(&target_desc);

    D3D11_VIEWPORT vp;
    vp.Width = target_desc.Width;
    vp.Height = target_desc.Height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    m_context->RSSetViewports(1, &vp);

	ID3D11RenderTargetView* rtv = target.GetRTV();
    const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    m_context->ClearRenderTargetView(rtv, clearColor);

    m_context->OMSetRenderTargets(1, &rtv, nullptr);

    // Set the vertex buffer
    m_context->IASetInputLayout(m_vertexLayout.Get());

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);

    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
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
