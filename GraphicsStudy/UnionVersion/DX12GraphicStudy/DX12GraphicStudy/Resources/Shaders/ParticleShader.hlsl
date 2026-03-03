cbuffer cbCameraInfo : register(b0)
{
    //matrix gProjection;
    matrix gView; //16
    float4 Camera_Position; // 20
    float4 emmitPosition; // 24
    float DecreaseRate; // 25
    float DeltaTime; // 26
};

Texture2D texDiffuse : register(t0);
SamplerState StaticSampler : register(s0);

struct PARTICLE_VERTEX
{
    float4 position : SV_POSITION;
    float3 velocity : NORMAL;
    float lifetime : NUMBER;
    uint type : ID;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 positionW : POSITION;
    float2 uv : TEXCOORD0;
};

PARTICLE_VERTEX VS_Particle(PARTICLE_VERTEX input)
{
    PARTICLE_VERTEX output;
    output = input;
    return (output);
}
 
[maxvertexcount(1)]
void GS_Particle_StreamOutput(point PARTICLE_VERTEX input[1], inout PointStream<PARTICLE_VERTEX> outStream)
{
    PARTICLE_VERTEX v;
    v.position = input[0].position + float4(input[0].velocity, 0);
    v.velocity = input[0].velocity;
    v.lifetime = input[0].lifetime - DeltaTime;
    if (v.lifetime <= 0)
    {
        v.position = emmitPosition;
        v.lifetime = 1.0f;
    }
    v.type = input[0].type;
    outStream.Append(v);
    outStream.RestartStrip();
}

VS_OUTPUT VS_DrawParticle(PARTICLE_VERTEX input)
{
    VS_OUTPUT v;
    v.positionW = input.position;
    v.position = mul(input.position, gView);
    v.uv = float2(0, 0);
    return v;
}

struct GS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float3 normalW : NORMAL;
    float4 positionW : POSITION;
    float2 uv : TEXCOORD;
};

[maxvertexcount(6)]
void GSMain(point VS_OUTPUT input[1], inout TriangleStream<VS_OUTPUT> outStream)
{
    VS_OUTPUT vert = input[0];
    VS_OUTPUT v0, v1, v2, v3;
    
    float3 up = float3(0, 1, 0);
    float3 viewDir = normalize(vert.positionW.xyz - Camera_Position.xyz);
    float3 bilRight = cross(up, viewDir);
    float3 bilUp = cross(bilRight, viewDir);
    v0 = vert;
    v1 = vert;
    v2 = vert;
    v3 = vert;
    
    v0.positionW.xyz += 10*bilUp;
    v1.positionW.xyz += 10*bilUp + 5*bilRight;
    v2.positionW.xyz += 5*bilRight;
    
    v0.uv = float2(0, 1);
    v1.uv = float2(1, 1);
    v2.uv = float2(1, 0);
    v3.uv = float2(0, 0);
    
    v0.position = mul(v0.positionW, gView);
    v1.position = mul(v1.positionW, gView);
    v2.position = mul(v2.positionW, gView);
    
    outStream.Append(v2);
    outStream.Append(v1);
    outStream.Append(v0);
    outStream.RestartStrip();
    outStream.Append(v3);
    outStream.Append(v2);
    outStream.Append(v0);
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