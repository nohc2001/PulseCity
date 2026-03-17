cbuffer cbCameraInfo : register(b0)
{
    //matrix gProjection;
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
};

float Sigmoid(float x)
{
    return 1.0 / (1.0 + (exp(-x)));
}

VS_OUTPUT VSMain(VS_INPUT input)
{
    VS_OUTPUT output;
    //output.position = mul(mul(mul(float4(input.position, 1.0f), gWorld), gView), gProjection);
    output.position = mul(mul(float4(input.position, 1.0f), gWorld), gView);
    output.color = input.color;
    return (output);
}

float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
    return input.color;
}