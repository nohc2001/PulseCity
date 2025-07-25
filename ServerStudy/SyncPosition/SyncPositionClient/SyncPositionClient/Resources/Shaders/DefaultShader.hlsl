cbuffer cbCameraInfo : register(b0)
{
    matrix gProjection;
    matrix gView;
};

cbuffer cbWorldInfo : register(b1)
{
    matrix gWorld;
};

struct VS_INPUT
{
    float3 position : POSITION;
    float4 color : COLOR;
    float3 normal : NORMAL;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float3 normalW : NORMAL;
    float4 positionW : POSITION;
};

VS_OUTPUT VSMain(VS_INPUT input)
{
    VS_OUTPUT output;
    output.positionW = mul(float4(input.position, 1.0f), gWorld);
    output.position = mul(mul(output.positionW, gView), gProjection);
    output.color = input.color;
    output.normalW = mul(input.normal, (float3x3) gWorld);
    return (output);
}
 //�ȼ� ���̴��� �����Ѵ�.
float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
    return (input.color);
}