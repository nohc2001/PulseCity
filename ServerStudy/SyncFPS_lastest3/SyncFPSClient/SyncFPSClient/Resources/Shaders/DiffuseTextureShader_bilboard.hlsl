cbuffer cbCameraInfo : register(b0)
{
    matrix gProjection;
    matrix gView;
    float4 Camera_Position;
};

cbuffer cbWorldInfo : register(b1)
{
    matrix gWorld;
};

struct DirectionLight
{
    float3 gLightDirection; // Light Direction
    float4 gLightColor; // Light_Color
};

struct PointLight
{
    float3 LightPos;
    float LightIntencity;
    float3 LightColor; // Light_Color
    float LightRange;
};

Texture2D texDiffuse : register(t0);
SamplerState StaticSampler : register(s0);

struct VS_INPUT
{
    float3 position : POSITION;
    float4 color : COLOR;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float3 normalW : NORMAL;
    float4 positionW : POSITION;
    float3 cameraPosW : TEXCOORD0;
    float2 uv : TEXCOORD1;
};

float Sigmoid(float x)
{
    return 1.0 / (1.0 + (exp(-x)));
}

VS_OUTPUT VSMain(VS_INPUT input)
{
    VS_OUTPUT output;
    output.positionW = mul(float4(input.position, 1.0f), gWorld);
    output.position = mul(mul(output.positionW, gView), gProjection);
    output.color = input.color;
    output.normalW = mul(input.normal, (float3x3) gWorld);
    output.uv = input.uv;
    return (output);
}

struct GS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float3 normalW : NORMAL;
    float4 positionW : POSITION;
    float2 uv : TEXCOORD1;
};


struct GS_OUTPUT_PARTICLE
{
    float4 position : SV_POSITION; // screen pos
    float4 color : COLOR; // color
    float3 normalW : NORMAL; // move direction
    float4 positionW : POSITION; // world pos
    float2 uv : TEXCOORD1; // texuv
    uint ID : SV_GSInstanceID;
};

[maxvertexcount(1)]
void GS_StreamOutput(point GS_OUTPUT_PARTICLE input[1], inout PointStream<VS_OUTPUT> outStream)
{
    VS_OUTPUT v;
    v.position = input[0].position;
    v.positionW = input[0].positionW;
    v.color = input[0].color;
    v.normalW = input[0].normalW;
    v.uv = input[0].uv;
    
    v.positionW.xyz += v.normalW;
    v.normalW.y -= 0.017f; // 60hz
    v.position = mul(mul(v.positionW, gView), gProjection);
    if (v.position.y < -1)
    {
        v.positionW.xyz = 
        v.positionW = mul(float4(float3(0, 0, 0), 1.0f), gWorld);
        v.position = mul(mul(v.positionW, gView), gProjection);
        v.normalW = float3(cos((float) (input[0].ID) * 10.0f), 10, sin((float) (input[0].ID) * 10.0f));
    }
    outStream.Append(v);
}

[maxvertexcount(6)]
void GSMain(point VS_OUTPUT input[1], inout TriangleStream<VS_OUTPUT> outStream)
{
    VS_OUTPUT vert = input[0];
    VS_OUTPUT v[4];
    
    float3 up = float3(0, 1, 0);
    float3 viewDir = normalize(vert.positionW.xyz - Camera_Position.xyz);
    float3 bilRight = cross(up, viewDir);
    float3 bilUp = cross(bilRight, viewDir);
    v[0] = vert;
    v[1] = vert;
    v[2] = vert;
    v[3] = vert;
    
    v[0].positionW.xyz += 10*bilUp;
    v[1].positionW.xyz += 10*bilUp + 5*bilRight;
    v[2].positionW.xyz += 5*bilRight;
    
    v[0].uv = float2(0, 1);
    v[1].uv = float2(1, 1);
    v[2].uv = float2(1, 0);
    v[3].uv = float2(0, 0);
    
    v[0].position = mul(mul(v[0].positionW, gView), gProjection);
    v[1].position = mul(mul(v[1].positionW, gView), gProjection);
    v[2].position = mul(mul(v[2].positionW, gView), gProjection);
    
    outStream.Append(v[2]);
    outStream.Append(v[1]);
    outStream.Append(v[0]);
    outStream.RestartStrip();
    outStream.Append(v[3]);
    outStream.Append(v[2]);
    outStream.Append(v[0]);
    outStream.RestartStrip();
}

 //«»ºø ºŒ¿Ã¥ı∏¶ ¡§¿««—¥Ÿ.
float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
    float4 col = texDiffuse.Sample(StaticSampler, input.uv);
    if (col.a <= 0.0001)
        discard;
    return col;
}