#include "fullscreenquad.h"
#include "d3dcompiler.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

using namespace DirectX;

const char* QUAD_VS = R"(

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

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT2 texCoord;
};

FullScreenQuad::FullScreenQuad()
{
}

FullScreenQuad::~FullScreenQuad()
{
}

HRESULT FullScreenQuad::Initialize(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context)
{
    HRESULT hr = S_OK;
    if (m_device != device)
    {
        m_vertexShader = nullptr;
    }

    m_device = device;
    m_context = context;

    if (!m_vertexShader)
    {
        ID3DBlob* errorBlob = nullptr;

        // Compile and create vertex shader
        ID3DBlob* vsBlob = nullptr;
        hr = D3DCompile(QUAD_VS, strlen(QUAD_VS), "VS", nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, &errorBlob);
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
    }

    return hr;
}

HRESULT FullScreenQuad::Render(ID3D11DeviceContext* context)
{
    // Set the vertex buffer
    context->IASetInputLayout(m_vertexLayout.Get());

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);

    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    context->VSSetShader(m_vertexShader.Get(), nullptr, 0);

    return S_OK;
}
