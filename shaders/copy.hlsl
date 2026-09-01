cbuffer CopyParams : register(b0)
{
    float2 srcOffset; // Top-left of source region (in pixels)
    float2 srcSize; // Width/Height of source region (in pixels)
    float2 dstOffset; // Top-left of destination region (in pixels)
    float2 dstSize; // Width/Height of destination region (in pixels)
    uint flipHorizontal; // 1 to flip, 0 otherwise
    uint flipVertical; // 1 to flip, 0 otherwise
    float blend;
    float padding; // Align to 16 bytes
};

Texture2D<float4> gInput : register(t0);
Texture2D<float4> gPrevious : register(t1);
RWTexture2D<float4> gOutput : register(u0);
SamplerState samLinear : register(s0);

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 outCoord = (uint2) (DTid.xy + dstOffset);
    
    uint width, height;
    gOutput.GetDimensions(width, height);

    if (outCoord.x >= width || outCoord.y >= height)
    {
        return;
    }
    
    if (DTid.x >= (uint) dstSize.x || DTid.y >= (uint) dstSize.y)
    {
        return;
    }
    
    float2 uv = (float2) DTid / dstSize;
    
    if (flipHorizontal > 0)
        uv.x = 1.0f - uv.x;
    if (flipVertical > 0)
        uv.y = 1.0f - uv.y;
    
    uint srcWidth, srcHeight;
    gInput.GetDimensions(srcWidth, srcHeight);
    float2 texelSize = 1.0f / float2(srcWidth, srcHeight);

    // Calculate source region in normalized coordinates
    float2 srcUVStart = srcOffset * texelSize;
    float2 srcUVSize = srcSize * texelSize;
    
    // Final UV within the source texture
    float2 finalUV = srcUVStart + (uv * srcUVSize);

    // 6. Sample with bilinear interpolation and blend with the previous frame
    float4 current = gInput.SampleLevel(samLinear, finalUV, 0);
    [branch]
    if (blend >= 1.0f)
        gOutput[outCoord.xy] = current;
    else
        gOutput[outCoord.xy] = lerp(gPrevious.Load(int3(outCoord, 0)), current, blend);
}
