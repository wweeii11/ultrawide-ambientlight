cbuffer CopyParams : register(b0)
{
    uint flipMode; // 0 = normal, 1 = horizontal flip, 2 = vertical flip
    float2 padding; // align to 16 bytes (unused)
};

Texture2D<float4> gInput : register(t0);
RWTexture2D<float4> gOutput : register(u0);
SamplerState samLinear : register(s0);

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint width, height;
    gOutput.GetDimensions(width, height);

    if (DTid.x >= width || DTid.y >= height)
        return;

    float2 texCoord = (float2(DTid.xy) + 0.5f) / float2(width, height);

    // Apply flip modes
    if (flipMode == 1)        // horizontal
        texCoord.x = 1.0 - texCoord.x;
    else if (flipMode == 2)   // vertical
        texCoord.y = 1.0 - texCoord.y;

    // Bilinear sampling from source texture
    float4 color = gInput.SampleLevel(samLinear, texCoord, 0);

    // Write to output
    gOutput[DTid.xy] = color;
}
