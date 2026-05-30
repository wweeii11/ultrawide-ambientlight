Texture2D<float> lumaTexture : register(t0);

#ifdef USE_UNORM
RWTexture2D<unorm float4> outputTexture : register(u0);
#else
RWTexture2D<float4> outputTexture : register(u0);
#endif

// --- Constant Buffer for Detection Thresholds ---
cbuffer DetectionParams : register(b0)
{
    uint Width;
    uint Height;
    float BlackThreshold;
    float BlackRatio;
    float VarianceThreshold;
};

// sample the luma texture and use it as an alpha mask for the rendered effects
// for luma < threshold, output original alpha (original is black, effect is visible)
// otherwise output 0.0 alpha (original is shown, effect hidden)

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint width, height;
    outputTexture.GetDimensions(width, height);

    if (DTid.x >= width || DTid.y >= height)
        return;
    
    lumaTexture.Load(int3(DTid.xy, 0));
    float luma = lumaTexture.Load(int3(DTid.xy, 0));
    float4 color = outputTexture.Load(int3(DTid.xy, 0));
    float alpha = luma < BlackThreshold ? 1.0 : 1.0 - pow(luma, 2.0);
    color = color * alpha;
    outputTexture[DTid.xy] = color;

}
