cbuffer cbCameraInfo : register(b0)
{
    matrix gView;
    float4 Camera_Position;
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
    float4 color : COLOR;
    float3 normalW : NORMAL;
    float3 positionW : POSITION;
};

VS_OUTPUT VSMain(VS_INPUT input)
{
    VS_OUTPUT output;
    output.positionW = mul(float4(input.position, 1.0f), gWorld);
    output.color = input.color;
    output.normalW = mul(input.normal, (float3x3) gWorld);
    return (output);
}

//stand:quad
struct HS_CONSTANT_OUTPUT
{
    float fTessEdges[4] : SV_TessFactor;
    float fTessInsides[2] : SV_InsideTessFactor;
};

//Patch Data Structure
//[(0.00, 0.00), (0.33, 0.00), (0.66, 0.00), (1.00, 0.00)]
//[(0.00, 0.33), (0.33, 0.33), (0.66, 0.33), (1.00, 0.33)]
//[(0.00, 0.66), (0.33, 0.66), (0.66, 0.66), (1.00, 0.66)]
//[(0.00, 1.00), (0.33, 1.00), (0.66, 1.00), (1.00, 1.00)]
HS_CONSTANT_OUTPUT HSConstant(InputPatch<VS_OUTPUT, 16> input, uint PatchID : SV_PrimitiveID)
{
    HS_CONSTANT_OUTPUT output;
    output.fTessEdges[0] = 8;
    output.fTessEdges[1] = 8;
    output.fTessEdges[2] = 8;
    output.fTessEdges[3] = 8;
    output.fTessInsides[0] = 8;
    output.fTessInsides[1] = 8;
    return output;
}

struct BEZIER_CONTROL_POINT
{
    float3 position : BEZIERPOS;
    float3 Normal : NORMAL;
    float4 color : COLOR;
};

[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(16)]
[patchconstantfunc("HSConstant")]
[maxtessfactor(64.0f)]
BEZIER_CONTROL_POINT HSBezier(InputPatch<VS_OUTPUT, 16> input
, uint i : SV_OutputControlPointID
, uint patchID : SV_PrimitiveID)
{
    BEZIER_CONTROL_POINT output;
    output.position = input[i].positionW;
    output.Normal = input[i].normalW;
    output.color = input[i].color;
    return output;
}

struct DS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 positionW : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR;
};

BEZIER_CONTROL_POINT GetBezier4(BEZIER_CONTROL_POINT cp[4], float rate)
{
    BEZIER_CONTROL_POINT r;
    float3 bx0 = lerp(cp[0].position, cp[1].position, rate);
    float3 bx1 = lerp(cp[1].position, cp[2].position, rate);
    float3 bx2 = lerp(cp[2].position, cp[3].position, rate);
    float3 bbx0 = lerp(bx0, bx1, rate);
    float3 bbx1 = lerp(bx1, bx2, rate);
    r.position = lerp(bbx0, bbx1, rate);
    bx0 = lerp(cp[0].Normal, cp[1].Normal, rate);
    bx1 = lerp(cp[1].Normal, cp[2].Normal, rate);
    bx2 = lerp(cp[2].Normal, cp[3].Normal, rate);
    bbx0 = lerp(bx0, bx1, rate);
    bbx1 = lerp(bx1, bx2, rate);
    r.Normal = lerp(bbx0, bbx1, rate);
    r.color = lerp(cp[0].color, cp[1].color, rate);
    return r;
}

[domain("quad")]
DS_OUTPUT DSBezier(HS_CONSTANT_OUTPUT hsconstant
, float2 uv : SV_DomainLocation
, OutputPatch<BEZIER_CONTROL_POINT, 16> patch)
{
    BEZIER_CONTROL_POINT cp[4];
    BEZIER_CONTROL_POINT cpy[4];
    cp[0] = patch[3];
    cp[1] = patch[2];
    cp[2] = patch[1];
    cp[3] = patch[0];
    cpy[0] = GetBezier4(cp, uv.x);
    cp[0] = patch[7];
    cp[1] = patch[6];
    cp[2] = patch[5];
    cp[3] = patch[4];
    cpy[1] = GetBezier4(cp, uv.x);
    cp[0] = patch[11];
    cp[1] = patch[10];
    cp[2] = patch[9];
    cp[3] = patch[8];
    cpy[2] = GetBezier4(cp, uv.x);
    cp[0] = patch[15];
    cp[1] = patch[14];
    cp[2] = patch[13];
    cp[3] = patch[12];
    cpy[3] = GetBezier4(cp, uv.x);
    BEZIER_CONTROL_POINT r = GetBezier4(cpy, uv.y);
    
    DS_OUTPUT output;
    output.positionW = float4(r.position, 1.0f);
    output.position = mul(output.positionW, gView);
    output.normal = r.Normal;
    output.color = r.color;
    return output;
}

//«»ºø ºŒ¿Ã¥ı∏¶ ¡§¿««—¥Ÿ.
float4 PSMain(DS_OUTPUT input) : SV_TARGET
{   
    float3 diffuseTexColor = input.color;
    
    float3 normalW = normalize(input.normal);
    float3 positionW = input.positionW.xyz;
    
    float3 LightDir = normalize(float3(1, 1, 1));
    float4 outcolor = dot(normalW, LightDir) * input.color;
    outcolor.a = 1.0f;
    return outcolor;
}