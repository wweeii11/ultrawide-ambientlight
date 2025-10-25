Texture2D<float> lumaTexture : register(t0);
RWTexture2D<unorm float4> outputTexture : register(u0);

// sample the luma texture and use it as an alpha mask
// for luma < 0.5, output original alpha, otherwise output 0.0 alpha

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
    float alpha = luma < 0.1 ? 1.0 : (1.0 - luma) / 4;
    color = color * alpha;
    outputTexture[DTid.xy] = color;

}
