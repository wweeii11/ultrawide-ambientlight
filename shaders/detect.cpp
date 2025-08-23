#include "detect.h"
#include "d3dcompiler.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

using namespace DirectX;

const char* DETECT_CS = R"(
cbuffer Constants : register(b0)
{
    uint  TextureWidth;
    uint  TextureHeight;
    float BlackThreshold;     // e.g., 0.05f for 5% brightness
    float BlackRatioRequired; // e.g., 0.98f = 98% pixels must be black
};

// Rec.709 luma weights
static const float3 LumaWeights = float3(0.2126, 0.7152, 0.0722);

Texture2D InputTexture : register(t0);
RWStructuredBuffer<uint> RowFlags : register(u0); // size = TextureHeight
RWStructuredBuffer<uint> ColFlags : register(u1); // size = TextureWidth

float3 SRGBToLinear(float3 srgb)
{
    // Fast sRGB → Linear approx (could replace with exact curve if needed)
    return pow(srgb, 2.2);
}

bool IsBlackLuma(float3 srgb)
{
    //float3 linearRGB = SRGBToLinear(srgb);
    float3 linearRGB = srgb; // assuming input is already linear
    float luma = dot(linearRGB, LumaWeights);
    return (luma < BlackThreshold);
}

// Thread group: each thread processes 1 row and 1 column
[numthreads(256, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint index = id.x;

    // --- Row check ---
    if (index < TextureHeight)
    {
        uint blackCount = 0;
        for (uint x = 0; x < TextureWidth; ++x)
        {
            float3 color = InputTexture.Load(int3(x, index, 0)).rgb;
            if (IsBlackLuma(color))
                blackCount++;
        }

        float ratio = (float)blackCount / (float)TextureWidth;
        RowFlags[index] = (ratio >= BlackRatioRequired) ? 1 : 0;
    }

    // --- Column check ---
    if (index < TextureWidth)
    {
        uint blackCount = 0;
        for (uint y = 0; y < TextureHeight; ++y)
        {
            float3 color = InputTexture.Load(int3(index, y, 0)).rgb;
            if (IsBlackLuma(color))
                blackCount++;
        }

        float ratio = (float)blackCount / (float)TextureHeight;
        ColFlags[index] = (ratio >= BlackRatioRequired) ? 1 : 0;
    }
}

)";

struct DetectCB
{
	UINT TextureWidth;
	UINT TextureHeight;
	float BlackThreshold;     // e.g. 0.05f
	float BlackRatioRequired; // e.g. 0.98f
};

Detection::Detection()
{
}

Detection::~Detection()
{
}

static void CreateStructuredBufferWithUAVAndStaging(
	ID3D11Device* dev,
	UINT elementCount,
	ID3D11Buffer** gpuBufOut,           // UAV-capable DEFAULT buffer
	ID3D11UnorderedAccessView** uavOut, // UAV
	ID3D11Buffer** stagingOut)          // STAGING copy for CPU read
{
	// UAV-capable GPU buffer
	D3D11_BUFFER_DESC bd = {};
	bd.ByteWidth = elementCount * sizeof(UINT);
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	bd.CPUAccessFlags = 0;
	bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	bd.StructureByteStride = sizeof(UINT);
	dev->CreateBuffer(&bd, nullptr, gpuBufOut);

	// UAV
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavd = {};
	uavd.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavd.Format = DXGI_FORMAT_UNKNOWN; // structured
	uavd.Buffer.NumElements = elementCount;
	dev->CreateUnorderedAccessView(*gpuBufOut, &uavd, uavOut);

	// Staging buffer
	D3D11_BUFFER_DESC sd = {};
	sd.ByteWidth = elementCount * sizeof(UINT);
	sd.Usage = D3D11_USAGE_STAGING;
	sd.BindFlags = 0;
	sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	sd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	sd.StructureByteStride = sizeof(UINT);
	dev->CreateBuffer(&sd, nullptr, stagingOut);
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

	CreateStructuredBufferWithUAVAndStaging(
		device.Get(),
		height,
		&m_rowBuf,
		&m_rowUAV,
		&m_rowStaging);
	CreateStructuredBufferWithUAVAndStaging(
		device.Get(),
		width,
		&m_colBuf,
		&m_colUAV,
		&m_colStaging);

	D3D11_BUFFER_DESC cbd = {};
	cbd.ByteWidth = sizeof(DetectCB);
	cbd.Usage = D3D11_USAGE_DEFAULT;
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	device->CreateBuffer(&cbd, nullptr, &m_detectCB);

	DetectCB detectData = {};
	detectData.TextureWidth = width;
	detectData.TextureHeight = height;
	detectData.BlackThreshold = blackThreshold;
	detectData.BlackRatioRequired = blackRatio;

	m_context->UpdateSubresource(
		m_detectCB.Get(), 0, nullptr, &detectData, sizeof(DetectCB), 0);

	m_width = width;
	m_height = height;
	return hr;
}

HRESULT Detection::Detect(TextureView target)
{
	HRESULT hr = S_OK;

	D3D11_TEXTURE2D_DESC target_desc = {};
	target.GetTexture()->GetDesc(&target_desc);

	// Set up the constant buffer
	m_context->CSSetConstantBuffers(0, 1, m_detectCB.GetAddressOf());

	// Set the shader
	m_context->CSSetShader(m_computeShader.Get(), nullptr, 0);

	// Set the input texture
	ID3D11ShaderResourceView* srv = target.GetSRV();
	m_context->CSSetShaderResources(0, 1, &srv);

	// Set the UAVs for row and column flags
	ID3D11UnorderedAccessView* uavs[] = { m_rowUAV.Get(), m_colUAV.Get() };
	m_context->CSSetUnorderedAccessViews(0, 2, uavs, nullptr);

	// Dispatch compute shader to process rows and columns
	UINT threadGroupCountX = (target_desc.Width + 255) / 256;
	UINT threadGroupCountY = 1; // Only one row/column processed at a time
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

	m_context->CopyResource(m_rowStaging.Get(), m_rowBuf.Get());
	m_context->CopyResource(m_colStaging.Get(), m_colBuf.Get());

	D3D11_MAPPED_SUBRESOURCE mRow = {}, mCol = {};
	m_context->Map(m_rowStaging.Get(), 0, D3D11_MAP_READ, 0, &mRow);
	m_context->Map(m_colStaging.Get(), 0, D3D11_MAP_READ, 0, &mCol);

	const UINT* rowFlags = reinterpret_cast<const UINT*>(mRow.pData); // size texH
	const UINT* colFlags = reinterpret_cast<const UINT*>(mCol.pData); // size texW


	m_topBarEnd = -1;
	for (UINT y = 0; y < UINT(m_height); ++y) {
		if (rowFlags[y] == 0) { m_topBarEnd = int(y) - 1; break; }
	}
	if (m_topBarEnd < 0) m_topBarEnd = 0; // all-black safety

	m_bottomBarStart = int(m_height);
	for (int y = int(m_height) - 1; y >= 0; --y) {
		if (rowFlags[y] == 0) { m_bottomBarStart = y + 1; break; }
	}

	m_leftBarEnd = -1;
	for (UINT x = 0; x < UINT(m_width); ++x) {
		if (colFlags[x] == 0) { m_leftBarEnd = int(x) - 1; break; }
	}
	if (m_leftBarEnd < 0) m_leftBarEnd = 0;

	m_rightBarStart = int(m_width);
	for (int x = int(m_width) - 1; x >= 0; --x) {
		if (colFlags[x] == 0) { m_rightBarStart = x + 1; break; }
	}

	//OutputDebugStringA((std::to_string(m_topBarEnd) + "," + std::to_string(m_bottomBarStart) + "," +
	//    std::to_string(m_leftBarEnd) + "," + std::to_string(m_rightBarStart) + "\n").c_str());

	m_context->Unmap(m_rowStaging.Get(), 0);
	m_context->Unmap(m_colStaging.Get(), 0);

	int bar_width = min(m_leftBarEnd, m_width - m_rightBarStart) + 1;
	m_detectWidth = m_width - bar_width * 2;

	int bar_height = min(m_topBarEnd, m_height - m_bottomBarStart) + 1;
	m_detectHeight = m_height - bar_height * 2;

	// allow auto detection to go up to 3/4 of screen
	if (m_detectWidth < m_width / 4 || m_detectWidth > m_width - 4)
	{
		m_detectWidth = m_width;
	}
	if (m_detectHeight < m_height / 4 || m_detectHeight > m_height - 4)
	{
		m_detectHeight = m_height;
	}

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

int Detection::GetWidth()
{
	return m_detectWidth;
}

int Detection::GetHeight()
{
	return m_detectHeight;
}
