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

cbuffer DepthInfoCB : register(b3)
{
    float4 CameraPosUseRayDepth;
};

Texture2D DiffuseTex : register(t0);
Texture2D RayDepthTex : register(t1);
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
    float3 WorldPos : TEXCOORD1;
};

VSOut VSMain(VSIn input)
{
    VSOut output;
    float4 worldPos = mul(float4(input.Position, 1.0f), World);
    output.Position = mul(worldPos, ViewProj);
    output.Color = input.Color;
    output.UV = input.UV * UVAnim.xy + UVAnim.zw;
    output.WorldPos = worldPos.xyz;
    return output;
}

float4 PSMain(VSOut input) : SV_Target
{
    if (CameraPosUseRayDepth.w > 0.5f)
    {
        int2 pixel = int2(input.Position.xy);
        uint depthWidth = 0;
        uint depthHeight = 0;
        RayDepthTex.GetDimensions(depthWidth, depthHeight);
        pixel = clamp(pixel, int2(0, 0), int2((int)depthWidth - 1, (int)depthHeight - 1));
        float sceneDepth = RayDepthTex.Load(int3(pixel, 0)).r;
        float effectDepth = saturate(length(input.WorldPos - CameraPosUseRayDepth.xyz) / 1000.0f);
        if (sceneDepth < 0.9999f && effectDepth > sceneDepth + 0.0015f)
        {
            discard;
        }
    }

    float4 tex = DiffuseTex.Sample(LinearSampler, input.UV);
    float brightness = max(max(tex.r, tex.g), tex.b);
    float glowMask = saturate((brightness - 0.045f) * 2.8f);
    tex.rgb *= 1.65f;
    tex.a = max(tex.a, glowMask) * glowMask * Tint.a;
    return float4(tex.rgb * Tint.rgb, tex.a) * input.Color;
}
