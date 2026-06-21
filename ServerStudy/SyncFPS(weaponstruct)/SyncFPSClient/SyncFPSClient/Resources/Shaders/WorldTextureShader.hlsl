cbuffer CameraCB : register(b0)
{
    float4x4 ViewProj;
};

cbuffer TransformCB : register(b1)
{
    float4x4 World;
};

cbuffer UVAnimCB : register(b2)
{
    float4 UVAnim;
};

cbuffer TintCB : register(b0)
{
    float4 Tint;
};

Texture2D DiffuseTex : register(t0);
SamplerState LinearSampler : register(s0);

struct VSIn
{
    float3 Position : POSITION;
    float4 Color : COLOR;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
};

struct VSOut
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
};

VSOut VSMain(VSIn input)
{
    VSOut output;
    float4 worldPos = mul(float4(input.Position, 1.0f), World);
    output.Position = mul(worldPos, ViewProj);
    output.Color = input.Color;
    output.UV = input.UV * UVAnim.xy + UVAnim.zw;
    return output;
}

float4 PSMain(VSOut input) : SV_Target
{
    float4 tex = DiffuseTex.Sample(LinearSampler, input.UV);
    float brightness = max(max(tex.r, tex.g), tex.b);
    float glowMask = saturate((brightness - 0.045f) * 2.8f);
    tex.rgb *= 1.65f;
    tex.a = max(tex.a, glowMask) * glowMask * Tint.a;
    return float4(tex.rgb * Tint.rgb, tex.a) * input.Color;
}
