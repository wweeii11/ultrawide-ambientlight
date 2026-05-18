Texture2D<float> lumaTexture : register(t0);
RWTexture2D<float4> outputTexture : register(u0);

static const float LumaThreshold = 0.03f; // Adjust as needed
static const float BlackPixelRatioThreshold = 0.7f; // 70% black to consider as a border

[numthreads(64, 1, 1)]
void mainh(uint3 dtid : SV_DispatchThreadID)
{
    uint width, height;
    lumaTexture.GetDimensions(width, height);

    uint index = dtid.x;

    // --- HORIZONTAL (Check Row 'index') ---
    if (index < height)
    {
        uint blackCount = 0;
        for (uint x = 0; x < width; ++x)
        {
            if (lumaTexture[uint2(x, index)] < LumaThreshold)
                blackCount++;
        }

        if ((float) blackCount / (float) width >= BlackPixelRatioThreshold)
        {
            for (uint x = 0; x < width; ++x)
            {
                outputTexture[uint2(x, index)] = float4(0.0f, 0.0f, 0.0f, 0.0f);
            }
        }
    }
}
[numthreads(64, 1, 1)]
void mainv(uint3 dtid : SV_DispatchThreadID)
{
    uint width, height;
    lumaTexture.GetDimensions(width, height);
    
    uint index = dtid.x;
    
    // --- VERTICAL (Check Column 'index') ---
    if (index < width)
    {
        uint blackCount = 0;
        for (uint y = 0; y < height; ++y)
        {
            if (lumaTexture[uint2(index, y)] < LumaThreshold)
                blackCount++;
        }

        if ((float) blackCount / (float) height >= BlackPixelRatioThreshold)
        {
            for (uint y = 0; y < height; ++y)
            {
                outputTexture[uint2(index, y)] = float4(0.0f, 0.0f, 0.0f, 0.0f);
            }
        }
    }
}
