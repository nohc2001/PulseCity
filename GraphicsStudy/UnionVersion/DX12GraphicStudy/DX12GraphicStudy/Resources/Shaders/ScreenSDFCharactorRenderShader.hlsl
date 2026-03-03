cbuffer LocationInfo : register(b0)
{
    float4 rect;
    float ScreenWidth;
    float ScreenHeight;
    float depth;
    float pad0;
    float4 Color;
    float MinD;
    float MaxD;
    //uint cbuf1[4]; //= { 0, 2, 2, 0};
    //uint cbuf2[4]; //= { 1, 1, 3, 3};
    //float cbuf3[4]; //= { 0, 1, 1, 0};
    //float cbuf4[4]; //= { 0, 0, 1, 1};
};

Texture2D texDiffuse : register(t0);
SamplerState StaticSampler : register(s0);

struct VS_INPUT
{
    float3 position : POSITION;
    float4 color : COLOR;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    uint vertexId : SV_VertexID;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

float Sigmoid(float x)
{
    return 1.0 / (1.0 + (exp(-x)));
}

VS_OUTPUT VSMain(VS_INPUT input)
{
    VS_OUTPUT output;
    output.color = input.color;
    float2 screenSize = float2(ScreenWidth, ScreenHeight);
    float4 rt = rect;
    float width = rt.z - rt.x;
    width *= 0.5f;
    float height = rt.w - rt.y;
    height *= 0.5f;
    if (input.vertexId == 0)
    {
        output.uv = float2(0, 0);
        float2 ndcPos = (float2(rt.x - width, rt.y-height) / screenSize);
        output.position = float4(ndcPos, depth, 1.0);
    }
    else if (input.vertexId == 1)
    {
        output.uv = float2(1, 0);
        float2 ndcPos = (float2(rt.z + width, rt.y - height) / screenSize);
        output.position = float4(ndcPos, depth, 1.0);
    }
    else if (input.vertexId == 2)
    {
        output.uv = float2(1, 1);
        float2 ndcPos = (float2(rt.z + width, rt.w + height) / screenSize);
        output.position = float4(ndcPos, depth, 1.0);
    }
    else if (input.vertexId == 3)
    {
        output.uv = float2(0, 1);
        float2 ndcPos = (float2(rt.x - width, rt.w + height) / screenSize);
        output.position = float4(ndcPos, depth, 1.0);
    }
    return output;
}

 //«»ºø ºŒ¿Ã¥ı∏¶ ¡§¿««—¥Ÿ.
float4 PSMain(VS_OUTPUT input) : SV_TARGET
{   
    float distance = texDiffuse.Sample(StaticSampler, input.uv).r;
    if (distance < 0)
        distance = -1.0 - distance;
    
    if (MinD < distance && distance < MaxD)
    {
        return Color;
    }
    else
    {
        discard;
        return float4(0, 0, 0, 1);
    }
}