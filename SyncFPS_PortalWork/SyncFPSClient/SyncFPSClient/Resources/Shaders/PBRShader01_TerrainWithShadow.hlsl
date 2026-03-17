cbuffer cbCameraInfo : register(b0)
{
    //matrix gProjection;
    matrix gView;
    float4 Camera_Position;
};

cbuffer cbWorldInfo : register(b1)
{
    matrix gWorld;
    float Tiling_base;
    float Tiling1;
    float Tiling2;
    float Tiling3;
    float Tiling4;
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

cbuffer cbLightInfo : register(b2)
{
    DirectionLight dirLight;
    PointLight pointLightArr[8];
    
    matrix LightProjection;
    matrix LightView;
    float3 LightPos;
}

//Texture2D Base_Tex : register(t0);
//Texture2D ColorPick : register(t1);
//Texture2D LightShadowMap : register(t2);
/*
t0 : color
t1 : normal
t2 : AO
t3 : metalic
t4 : Roughness
*/

// Ambient Occulusion

SamplerState StaticSampler : register(s0);

struct VS_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float3 tangent : TANGENT;
};

float Sigmoid(float x)
{
    return 1.0 / (1.0 + (exp(-x)));
}

//function from LearnOpenGL.

float Dfunction(float3 N, float3 H, float a)
{
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0);
    float NdotH2 = NdotH * NdotH;
    float nom = a2;
    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    denom = 3.141592 * denom * denom;
    return nom / denom;
}

float GeometrySchlinkGGX(float NdotV, float k)
{
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / denom;
}

float Gfunction(float3 N, float3 V, float3 L, float k)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlinkGGX(NdotV, k);
    float ggx2 = GeometrySchlinkGGX(NdotL, k);
    return ggx1 * ggx2;
}

float Ffunction(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 10.0);
}

float K_Direct(float Roughness)
{
    float r = (Roughness + 1);
    return r * r / 8;
}

float K_IBL(float Roughness)
{
    return Roughness * Roughness / 2;
}

float BRDF_cookToorrance(float3 Pos, float3 Wi, float3 ViewDir,
    float3 N, float Roughness, float Frenel)
{
    float Half = normalize(N + ViewDir);
    float D = Dfunction(N, Half, Roughness);
    float G = Gfunction(N, ViewDir, Wi, K_Direct(Roughness));
    return D * Frenel * G / (4 * dot(ViewDir, N) * dot(Wi, N));
}

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float3 normalW : NORMAL;
    float4 positionW : POSITION;
    //float3 cameraPosW : TEXCOORD0;
    float2 uv : TEXCOORD0;
    float3 ViewDir : TEXCOORD1; // View to Fragment
    float3 T : TEXCOORD2;
    float3 B : TEXCOORD3;
    float3 N : TEXCOORD4;
};

VS_OUTPUT VSMain(VS_INPUT input)
{
    VS_OUTPUT output;
    output.positionW = mul(float4(input.position, 1.0f), gWorld);
    output.position = mul(output.positionW, gView);
    output.normalW = mul(input.normal, (float3x3) gWorld);
    float4 cameraPosW = mul(Camera_Position, gWorld);
    output.uv = input.uv;
    //output.uv_base.x = Tiling_base * (input.uv.x);
    //output.uv_base.y = Tiling_base * (input.uv.y);
    //output.uv_1.x = Tiling1 * (input.uv.x);
    //output.uv_1.y = Tiling1 * (input.uv.y);
    //output.uv_2.x = Tiling2 * (input.uv.x);
    //output.uv_2.y = Tiling2 * (input.uv.y);
    //output.uv_3.x = Tiling3 * (input.uv.x);
    //output.uv_3.y = Tiling3 * (input.uv.y);
    //output.uv_4.x = Tiling4 * (input.uv.x);
    //output.uv_4.y = Tiling4 * (input.uv.y);
    float3 bitangent = cross(input.normal, input.tangent);
    
    float3 T = normalize(mul(input.tangent, (float3x3) gWorld));
    float3 B = normalize(mul(bitangent, (float3x3) gWorld));
    float3 N = normalize(mul(input.normal, (float3x3) gWorld));
    
    float3x3 TBN = transpose(float3x3(T, B, N));
    output.T = T;
    output.B = B;
    output.N = N;
    //output.TBNPointLightDir = mul(pointLightArr[0].LightPos - output.positionW.xyz, inverseTBN);
    //output.TBNDirLightDir = mul(dirLight.gLightDirection, inverseTBN);
    output.ViewDir = normalize(Camera_Position.xyz - output.positionW.xyz);
    
    return (output);
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / (3.141592 * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;

    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float4 OuterPBR(VS_OUTPUT input)
{
    //float4 Picker = ColorPick.Sample(StaticSampler, input.uv);
    //float sum = Picker.x + Picker.y + Picker.z + Picker.w;
    //Picker *= (1.0 / max(sum, 1.0f));
    //float base = 1.0 - sum;
    
    //float3 Color = /*base * */ Base_Tex.Sample(StaticSampler, input.uv);
    //+ Picker.x * Base_Tex[5].Sample(StaticSampler, input.uv_1)
    //+ Picker.y * Base_Tex[10].Sample(StaticSampler, input.uv_2)
    //+ Picker.z * Base_Tex[15].Sample(StaticSampler, input.uv_3)
    //+ Picker.w * Base_Tex[20].Sample(StaticSampler, input.uv_4);
    
    return float4(1, 1, 1, 1);
    
    //float3 TBNnormal = base * Base_Tex[1].Sample(StaticSampler, input.uv_base)
    //+ Picker.x * Base_Tex[6].Sample(StaticSampler, input.uv_1)
    //+ Picker.y * Base_Tex[11].Sample(StaticSampler, input.uv_2)
    //+ Picker.z * Base_Tex[16].Sample(StaticSampler, input.uv_3)
    //+ Picker.w * Base_Tex[21].Sample(StaticSampler, input.uv_4);
    //TBNnormal = TBNnormal.xzy;
    //TBNnormal = 2.0 * (TBNnormal - 0.5);
    
    //float AmbientOculusion = base * Base_Tex[2].Sample(StaticSampler, input.uv_base).r
    //+ Picker.x * Base_Tex[7].Sample(StaticSampler, input.uv_1).r
    //+ Picker.y * Base_Tex[12].Sample(StaticSampler, input.uv_2).r
    //+ Picker.z * Base_Tex[17].Sample(StaticSampler, input.uv_3).r
    //+ Picker.w * Base_Tex[22].Sample(StaticSampler, input.uv_4).r;
    
    //float Metalic = 0;
    
    //float Roughness = base * Base_Tex[3].Sample(StaticSampler, input.uv_base).r
    //+ Picker.x * Base_Tex[8].Sample(StaticSampler, input.uv_1).r
    //+ Picker.y * Base_Tex[13].Sample(StaticSampler, input.uv_2).r
    //+ Picker.z * Base_Tex[18].Sample(StaticSampler, input.uv_3).r
    //+ Picker.w * Base_Tex[23].Sample(StaticSampler, input.uv_4).r;
    
    //float3x3 invTBN = transpose(float3x3(input.T, input.B, input.N));
    //float3 normalW = normalize(mul(TBNnormal, invTBN));

    //float3 N = normalW;
    //float3 V = input.ViewDir;
    //float3 color = float3(0, 0, 0);
    
    //for (int i = 0; i < 8; ++i)
    //{
    //    PointLight p = pointLightArr[i];
    //    float3 L = normalize(p.LightPos - input.positionW.xyz);
    //    float3 H = normalize(V + L);

    //    float3 F0 = float3(0.04, 0.04, 0.04); // default for dielectrics
    //    F0 = lerp(F0, Color, Metalic);

    //    // Cook-Torrance BRDF
    //    float NDF = DistributionGGX(N, H, Roughness);
    //    float G = GeometrySmith(N, V, L, Roughness);
    //    float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    //    float3 numerator = NDF * G * F;
    //    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;
    //    float3 specular = numerator / denominator;

    //    float3 kS = F;
    //    float3 kD = 1.0 - kS;
    //    kD *= 1.0 - Metalic;

    //    float NdotL = max(dot(N, L), 0.0);
    //    float3 irradiance = p.LightColor * NdotL;

    //    float3 diffuse = kD * Color / 3.141592f;
    //    color += (diffuse + specular) * irradiance;
    //}
    
    //// Apply ambient occlusion
    //color = color * AmbientOculusion;

    //// Gamma correction
    //color = pow(color, 1.0 / 2.2);
    //color = color * 0.9 + Color * 0.1;
    
    //return float4(color, 1.0);
}

#define SHADOW_DEPTH_BIAS 0.000005

 //ÇÈ¼¿ ¼ÎÀÌ´õ¸¦ Á¤ÀÇÇÑ´Ù.
float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
    return float4(1, 1, 1, 1);
    //return OuterPBR(input);
    
    //// Compute pixel position in light space.
    //float4 vLightSpacePos = input.positionW;
    //vLightSpacePos = mul(vLightSpacePos, LightView);
    //vLightSpacePos = mul(vLightSpacePos, LightProjection);
    //vLightSpacePos.xyz /= vLightSpacePos.w;
    
    //// Translate from homogeneous coords to texture coords.
    //float2 vShadowTexCoord = 0.5f * vLightSpacePos.xy + 0.5f;
    //vShadowTexCoord.y = 1.0f - vShadowTexCoord.y;
    
    //float depth = LightShadowMap.Sample(StaticSampler, vShadowTexCoord).r;
    //float vLightSpaceDepth = vLightSpacePos.z;
    
    //float4 col = OuterPBR(input);
    //if (abs(depth - vLightSpaceDepth) > SHADOW_DEPTH_BIAS)
    //{
    //    return float4(col.xyz * 0.25, 1);
    //    //return float4(1, 1, 1, 1);
    //}
    //return col;
}