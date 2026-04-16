cbuffer cbCameraInfo : register(b0)
{
    matrix gView;
    float4 Camera_Position;
};

cbuffer cbDirLightInfo : register(b1)
{
    matrix DirLightProjection;
    matrix DirLightView;
    float3 DirLightPos;
    float3 DirLightDir;
    float4 DirLightColor;
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

struct Material
{
    bool AlphaBlendingMode; // opaque, transparent, cutout, ...
    float smoothness;
};

struct stMaterial
{
    float4 baseColor;
    float4 ambientColor;
    float4 emissiveColor;
    float metalicFactor;
    float smoothness;
    float bumpScaling;
    uint diffuseTexId;
    
    uint normalTexId;
    uint AOTexId;
    uint roughnessTexId;
    uint metalicTexId;
    
    float TilingX;
    float TilingY;
    float TilingOffsetX;
    float TilingOffsetY;
};

struct RenderInstance
{
    float4x4 world;
    uint MaterialIndex;
    uint3 extra; // 나중에 여기에 Point/Spot LightID 등을 넣자.
};

StructuredBuffer<RenderInstance> renderInstance : register(t0);
StructuredBuffer<stMaterial> Materials : register(t1);
Texture2D LightShadowMap : register(t2);

//#define MAX_TEXTURES 1024
Texture2D MaterialTexArr[MAX_TEXTURES] : register(t3);

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
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float extra : EXTRA;
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

float3 Ffunction(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
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

float3 BRDF_cookToorrance(float3 Pos, float3 Wi, float3 ViewDir,
    float3 N, float Roughness, float3 Frenel)
{
    float3 Half = normalize(Wi + ViewDir);
    float D = Dfunction(N, Half, Roughness);
    float G = Gfunction(N, ViewDir, Wi, K_Direct(Roughness));
    float denom = 4 * max(dot(ViewDir, N), 0.0) * max(dot(Wi, N), 0.0) + 1e-5;
    return D * Frenel * G / denom;
}

float3 BRDF_lambert(float3 color)
{
    return color / 3.141592f;
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
    uint instanceID : INSTANCEID;
};

VS_OUTPUT VSInstancingMain(VS_INPUT input, uint nInstanceID : SV_InstanceID)
{
    VS_OUTPUT output;
    matrix gWorld = renderInstance[nInstanceID].world;
    //gWorld = transpose(gWorld);
    stMaterial mat = Materials[renderInstance[nInstanceID].MaterialIndex];
    
    output.positionW = mul(float4(input.position, 1.0f), gWorld);
    output.position = mul(output.positionW, gView);
    output.normalW = mul(input.normal, (float3x3) gWorld);
    float4 cameraPosW = mul(Camera_Position, gWorld);
    output.uv.x = mat.TilingX * (input.uv.x + mat.TilingOffsetX);
    output.uv.y = mat.TilingY * (input.uv.y + mat.TilingOffsetY);
    float3 bitangent = cross(input.normal, input.tangent);
    
    float3 T = normalize(mul(input.tangent, (float3x3) gWorld));
    float3 B = normalize(mul(bitangent, (float3x3) gWorld));
    float3 N = normalize(mul(input.normal, (float3x3) gWorld));
    
    float3x3 TBN = transpose(float3x3(T, B, N));
    output.T = T;
    output.B = B;
    output.N = N;
    output.ViewDir = normalize(Camera_Position.xyz - output.positionW.xyz);
    output.instanceID = nInstanceID;
    return (output);
}

float4 PBRPS(VS_OUTPUT input)
{
    stMaterial mat = Materials[renderInstance[input.instanceID].MaterialIndex];
    uint idx = mat.diffuseTexId;
    idx = max(min(mat.diffuseTexId, MAX_TEXTURES - 1), 0);
    float3 Color = MaterialTexArr[idx].Sample(StaticSampler, input.uv) * mat.baseColor;
    idx = max(min(mat.normalTexId, MAX_TEXTURES - 1), 0);
    float3 TBNnormal = MaterialTexArr[mat.normalTexId].Sample(StaticSampler, input.uv);
    TBNnormal = TBNnormal.xyz;
    TBNnormal = 2.0 * (TBNnormal - 0.5);
    idx = max(min(mat.AOTexId, MAX_TEXTURES - 1), 0);
    float AmbientOculusion = MaterialTexArr[mat.AOTexId].Sample(StaticSampler, input.uv).r;
    idx = max(min(mat.metalicTexId, MAX_TEXTURES - 1), 0);
    float Metalic = MaterialTexArr[mat.metalicTexId].Sample(StaticSampler, input.uv).r;
    idx = max(min(mat.roughnessTexId, MAX_TEXTURES - 1), 0);
    float Roughness = min(0.5 + MaterialTexArr[mat.roughnessTexId].Sample(StaticSampler, input.uv).r, 1.0);
    
    float3 Frenel0 = float3(0.02, 0.02, 0.02);
    Frenel0 = lerp(Frenel0, Color, Metalic);
    
    float3 albedo = pow(Color, float3(2.2, 2.2, 2.2));
    float3x3 TBN = float3x3(input.T, input.B, input.N);
    float3x3 invTBN = transpose(TBN);
    float3 normalW = normalize(mul(TBNnormal, invTBN));
    //normalW = input.normalW;
    
    //PBR operation
    float sum = 0;
    float dW = 1.0 / 8;
    float3 Lo = float3(0.0, 0, 0);
    float3 diffuseColor = 0;
    //for (int i = 0; i < 8; ++i)
    //{
    //    PointLight p = pointLightArr[i];
    //    float3 wi = normalize(p.LightPos - input.positionW.xyz);
    //    float3 H = normalize(input.ViewDir.xyz + wi);
    //    float distance = length(p.LightPos - input.positionW.xyz);
    //    float attenuation = 1.0f / distance; //1.0 / (distance * distance);
        
    //    // Diffuse
    //    float3 lightVector = normalize(p.LightPos - input.positionW.xyz);
    //    float diffuse = saturate(dot(normalW, lightVector)); // dot normal & light direction 
    //    diffuseColor += p.LightColor * diffuse * albedo;
        
    //    float3 radiance = diffuse * p.LightIntencity * attenuation * p.LightColor;
        
    //    // Cook-Torrance BRDF
    //    float F = Ffunction(dot(H, wi), Frenel0);

    //    // scale light by NdotL
    //    float NdotL = max(dot(normalW, wi), 0.0);

    //    // add to outgoing radiance Lo
    //    Lo += dW * (BRDF_cookToorrance(input.positionW.xyz, wi, input.ViewDir.xyz, normalW, Roughness, F)) * radiance * NdotL;
    //}
    
    {
        float3 wi = normalize(-DirLightDir);
        
        float3 reflectV = reflect(wi, normalW);
        float specularRate = pow(max(dot(input.ViewDir.xyz, reflectV), 0), 5.0f) * Roughness; // PBR이 지금 안되어서 임시 스페큘러
        
        float3 H = normalize(input.ViewDir.xyz + wi);
        float3 radiance = DirLightColor.xyz;
        // Cook-Torrance BRDF
        float3 F = Ffunction(max(dot(H, wi), 0), Frenel0);
        // scale light by NdotL
        float NdotL = max(dot(normalW, wi), 0.0);
        // add to outgoing radiance Lo
        Lo += /* dW **/(BRDF_lambert(Color.xyz) + BRDF_cookToorrance(input.positionW.xyz, wi, input.ViewDir.xyz, normalW, Roughness, F) /*+ specularRate*/) * radiance * NdotL;
    }
    
    //diffuseColor = saturate(diffuseColor);
    float3 ambient = float3(0.03, 0.03, 0.03) * albedo * AmbientOculusion;
    float3 color = ambient + Lo;

    float exposure = 1; // 1.0보다 크면 더 밝음
    color = 1.0 - exp(-color * exposure); // exponential tone mapping
    // HDR tonemapping
    color = color / (color + float3(0.5f, 0.5f, 0.5f));
    // gamma correct
    color = pow(color, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
    return float4(color, 1.0);
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

#define SHADOW_DEPTH_BIAS 0.00005

 //픽셀 셰이더를 정의한다.
float4 PSInstancingMain(VS_OUTPUT input) : SV_TARGET
{
    //return float4(input.normalW, 1.0f);
    //float depth = LightShadowMap.Sample(StaticSampler, vShadowTexCoord).r;
    if (distance(Camera_Position, input.positionW) > 200)
    {
        return PBRPS(input);
    }
    else
    {
        // Compute pixel position in light space.
        float4 vLightSpacePos = input.positionW;
        vLightSpacePos = mul(vLightSpacePos, DirLightView);
        vLightSpacePos = mul(vLightSpacePos, DirLightProjection);
        vLightSpacePos.xyz /= vLightSpacePos.w;
        //vLightSpacePos.xy = vLightSpacePos.xy * 0.5f + 0.5f;

        float2 vShadowTexCoord = 0.5f * vLightSpacePos.xy + 0.5f;
        vShadowTexCoord.y = 1.0f - vShadowTexCoord.y;
        
        // Depth bias to avoid pixel self-shadowing.
        float vLightSpaceDepth = vLightSpacePos.z - SHADOW_DEPTH_BIAS;

        // Find sub-pixel weights.
        float2 vShadowMapDims = float2(4096, 4096); // need to keep in sync with .cpp file
        float4 vSubPixelCoords = float4(1.0f, 1.0f, 1.0f, 1.0f);
        vSubPixelCoords.xy = frac(vShadowMapDims * vShadowTexCoord);
        vSubPixelCoords.zw = 1.0f - vSubPixelCoords.xy;
        float4 vBilinearWeights = vSubPixelCoords.zxzx * vSubPixelCoords.wwyy;
        
        float2 vTexelUnits = 1.0f / vShadowMapDims;
        float4 vShadowDepths;
        vShadowDepths.x = LightShadowMap.Sample(StaticSampler, vShadowTexCoord);
        vShadowDepths.y = LightShadowMap.Sample(StaticSampler, vShadowTexCoord + float2(vTexelUnits.x, 0.0f));
        vShadowDepths.z = LightShadowMap.Sample(StaticSampler, vShadowTexCoord + float2(0.0f, vTexelUnits.y));
        vShadowDepths.w = LightShadowMap.Sample(StaticSampler, vShadowTexCoord + vTexelUnits);

        // What weighted fraction of the 4 samples are nearer to the light than this pixel?
        float4 vShadowTests = (vShadowDepths >= vLightSpaceDepth) ? 1.0f : 0.0f;
        float f = 0.25 * (vShadowTests.x + vShadowTests.y + vShadowTests.z + vShadowTests.w);
        //dot(vBilinearWeights, vShadowTests);
        
        float4 Color = PBRPS(input); //baseColor * PBR_Tex[0].Sample(StaticSampler, input.uv);
        if (f <= 0.001 /*abs(vShadowDepths - vLightSpaceDepth) > SHADOW_DEPTH_BIAS*/)
        {
            //PBR_Tex[0].Sample(StaticSampler, input.uv);
            return float4(Color.xyz * 0.2f, 1);
        }
        return float4((f * Color).xyz, 1);
    }
}