cbuffer cbCameraInfo : register(b0)
{
    matrix gView;
};

cbuffer cbWorldInfo : register(b1)
{
    matrix gWorld;
};

struct VS_SKYBOX_CUBEMAP_INPUT
{
    float3 position : POSITION;
};

struct VS_SKYBOX_CUBEMAP_OUTPUT
{
    float3 positionL : POSITION;
    float4 position : SV_POSITION;
};

VS_SKYBOX_CUBEMAP_OUTPUT VSSkyBox(VS_SKYBOX_CUBEMAP_INPUT input)
{
    VS_SKYBOX_CUBEMAP_OUTPUT output;

    output.position = mul(mul(float4(input.position, 1.0f), gWorld), gView);
    output.positionL = input.position;

    return (output);
}

TextureCube gtxtSkyCubeTexture : register(t0);
SamplerState StaticSampler : register(s0);

float4 PSSkyBox(VS_SKYBOX_CUBEMAP_OUTPUT input) : SV_TARGET
{
    float4 cColor = gtxtSkyCubeTexture.Sample(StaticSampler, input.positionL);
    return (cColor);
}