Texture2D<float4> InputTexture : register(t0);
RWTexture2D<float> OutputTexture : register(u0);

// --- Weights ---
static const float3 LumaWeights709 = float3(0.2126, 0.7152, 0.0722);
static const float3 LumaWeights2020 = float3(0.2627, 0.6780, 0.0593);

// --- Helpers ---

// PQ to Linear (for HDR10)
float3 ApplyPQDecode(float3 v)
{
    const float m1 = 0.1593017578125, m2 = 78.84375;
    const float c1 = 0.8359375, c2 = 18.8515625, c3 = 18.6875;
    float3 v_pow = pow(max(v, 0.00001), 1.0 / m2);
    return pow(max(v_pow - c1, 0.0) / (c2 - c3 * v_pow), 1.0 / m1);
}

// sRGB/Gamma to Linear (for all SDR: 8-bit or 10-bit)
float3 ApplySDRLinear(float3 srgb)
{
    return pow(max(srgb, 0.0), 2.2);
}

// --- Entry Points ---

// 1. SDR Entry Point (Handles B8G8R8A8 AND R10G10B10A2_SDR)
[numthreads(16, 16, 1)]
void mainSDR(uint3 id : SV_DispatchThreadID)
{
    uint2 size;
    InputTexture.GetDimensions(size.x, size.y);
    if (any(id.xy >= size))
        return;

    float4 color = InputTexture.Load(int3(id.xy, 0));
    float3 linearColor = ApplySDRLinear(color.rgb);
    
    // Output 0-1 range for black bar detection
    OutputTexture[id.xy] = saturate(dot(linearColor, LumaWeights709));
}

// 2. HDR10 Entry Point (R10G10B10A2 + PQ + BT.2020)
[numthreads(16, 16, 1)]
void mainHDR10(uint3 id : SV_DispatchThreadID)
{
    uint2 size;
    InputTexture.GetDimensions(size.x, size.y);
    if (any(id.xy >= size))
        return;

    float4 color = InputTexture.Load(int3(id.xy, 0));
    float3 linearColor = ApplyPQDecode(color.rgb);
    
    // Scale up so dark HDR content is detectable in R8_UNORM
    float luma = dot(linearColor, LumaWeights2020);
    OutputTexture[id.xy] = saturate(luma * 10.0);
}

// 3. scRGB Entry Point (R16G16B16A16 + Linear + BT.709)
[numthreads(16, 16, 1)]
void mainSCRGB(uint3 id : SV_DispatchThreadID)
{
    uint2 size;
    InputTexture.GetDimensions(size.x, size.y);
    if (any(id.xy >= size))
        return;

    float4 linearColor = InputTexture.Load(int3(id.xy, 0));
    float luma = dot(linearColor.rgb, LumaWeights709);
    
    // Standard scRGB 1.0 is 80 nits (SDR White)
    OutputTexture[id.xy] = saturate(luma);
}