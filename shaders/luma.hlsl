// Rec.709 luma weights
static const float3 LumaWeights = float3(0.2126, 0.7152, 0.0722);

Texture2D<float4> InputTexture : register(t0);
RWTexture2D<float> OutputTexture : register(u0);

float3 SRGBToLinear(float3 srgb)
{
    // Fast sRGB to Linear approx (could replace with exact curve if needed)
    return pow(srgb, 2.2);
}

// Thread group: each thread processes 1 row and 1 column
[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint width, height;
    InputTexture.GetDimensions(width, height);

    int2 coord = id.xy;
    if (coord.x >= width || coord.y >= height)
        return;

    float4 color = InputTexture.Load(int3(coord, 0));
    float luma = dot(color.rgb, LumaWeights);
    OutputTexture[coord] = luma;
}
