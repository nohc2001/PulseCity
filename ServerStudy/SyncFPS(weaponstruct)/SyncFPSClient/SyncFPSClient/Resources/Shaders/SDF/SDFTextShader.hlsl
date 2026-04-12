cbuffer LocationInfo : register(b0)
{
    float ScreenWidth;
    float ScreenHeight;
};

struct SDFInstance
{
    float4 rect;
    float4 uvrange;
    float4 Color;
    float depth;
    float MinD;
    float MaxD;
    uint pageId;
};

StructuredBuffer<SDFInstance> sdfInstances : register(t0);
#define MAX_TEXTURE 16
Texture2D SDFTextTextureArr[MAX_TEXTURE] : register(t1);
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
    float2 MinMaxDist : DISTANCE2;
    uint pageId : PAGEID;
};

float Sigmoid(float x)
{
    return 1.0 / (1.0 + (exp(-x)));
}

VS_OUTPUT VSMain(VS_INPUT input, uint nInstanceID : SV_InstanceID)
{
    SDFInstance ins = sdfInstances[nInstanceID];
    VS_OUTPUT output;
    output.color = ins.Color;
    float2 screenSize = float2(ScreenWidth, ScreenHeight);
    float4 rt = ins.rect;
    float width = rt.z - rt.x;
    width *= 0.5f;
    float height = rt.w - rt.y;
    height *= 0.5f;
    if (input.vertexId == 0)
    {
        float2 ndcPos = (float2(rt.x - width, rt.y - height) / screenSize);
        output.position = float4(ndcPos, ins.depth, 1.0);
        output.uv = float2(ins.uvrange.x, ins.uvrange.y);
    }
    else if (input.vertexId == 1)
    {
        float2 ndcPos = (float2(rt.z + width, rt.y - height) / screenSize);
        output.position = float4(ndcPos, ins.depth, 1.0);
        output.uv = float2(ins.uvrange.z, ins.uvrange.y);
    }
    else if (input.vertexId == 2)
    {
        output.uv = float2(ins.uvrange.z, ins.uvrange.w);
        float2 ndcPos = (float2(rt.z + width, rt.w + height) / screenSize);
        output.position = float4(ndcPos, ins.depth, 1.0);
    }
    else if (input.vertexId == 3)
    {
        output.uv = float2(ins.uvrange.x, ins.uvrange.w);
        float2 ndcPos = (float2(rt.x - width, rt.w + height) / screenSize);
        output.position = float4(ndcPos, ins.depth, 1.0);
    }
    output.pageId = ins.pageId;
    output.MinMaxDist = float2(ins.MinD, ins.MaxD);
    return output;
}

//픽셀 셰이더를 정의한다.
float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
    float distance = SDFTextTextureArr[input.pageId].Sample(StaticSampler, input.uv).r;
    //return float4(distance, distance, distance, 1);
    //if (distance < 0)
    //    distance = -1.0 - distance;
    distance = distance * 2 - 1;
    
    if (input.MinMaxDist.x < distance && distance < input.MinMaxDist.y)
    {
        return input.color;
    }
    else
    {
        discard;
        return float4(0, 0, 0, 1);
    }
}