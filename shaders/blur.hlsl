#define MAX_SAMPLE_COUNT 63

cbuffer BlurParams : register(b0)
{
    float SampleCount;
    float2 SampleOffsets[MAX_SAMPLE_COUNT];
    float SampleWeights[MAX_SAMPLE_COUNT];
};

Texture2D<float4> gInput : register(t0);
RWTexture2D<float4> gOutput : register(u0);
SamplerState gSampler : register(s0);

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 texSize;
    gOutput.GetDimensions(texSize.x, texSize.y);

    if (DTid.x >= texSize.x || DTid.y >= texSize.y)
        return;

    float2 texCoord = (float2(DTid.xy) + 0.5) / float2(texSize);

    float4 c = 0;
    int count = (int) SampleCount;

    // Same logic as pixel shader version
    [loop]
    for (int i = 0; i < count; i++)
    {
        c += gInput.SampleLevel(gSampler, texCoord + SampleOffsets[i], 0) * SampleWeights[i];
    }

    gOutput[DTid.xy] = c;
}
