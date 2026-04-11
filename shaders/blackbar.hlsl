Texture2D<float> lumaTexture : register(t0);
RWTexture2D<unorm float4> outputTexture : register(u0);

static const float LumaThreshold = 0.03f; // Adjust as needed
static const float BlackPixelRatioThreshold = 0.7f; // 70% black to consider as a border

[numthreads(16, 16, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint width, height;
    lumaTexture.GetDimensions(width, height);

    uint index = dtid.x;
    if (dtid.x == 0)
    {
        index = dtid.y;
    }
    
    if (index == 0) // Skip the first index, 0 is used to indicate whether we're processing a row or a column
        return;
    
    index -= 1;

    // --- HORIZONTAL (Check Row 'index') ---
    if (dtid.x == 0)
    {
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
            // If the row is 80% black, clear alpha for the whole row
                for (uint x = 0; x < width; ++x)
                {
                    outputTexture[uint2(x, index)] = float4(0.0f, 0.0f, 0.0f, 0.0f);
                }
            }
        }
        return;
    }
    
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
        // If the column is 90% black, clear alpha for the whole column
            for (uint y = 0; y < height; ++y)
            {
                outputTexture[uint2(index, y)] = float4(0.0f, 0.0f, 0.0f, 0.0f);
            }
        }
    }
}
