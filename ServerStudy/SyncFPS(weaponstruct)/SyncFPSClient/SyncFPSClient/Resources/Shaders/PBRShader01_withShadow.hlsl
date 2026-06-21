//#define DEBUG_NORMAL
#define ADD_PONG_SPECULAR
//#define DEBUG_ENVIRONMENTLIGHT

// ûũ ũ�Ⱑ �޶����� �ٲپ�� ��.
#define CHUNCK_WIDTH 50
cbuffer cbCameraInfo : register(b0)
{
    //matrix gProjection;
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

struct Material
{
    bool AlphaBlendingMode; // opaque, transparent, cutout, ...
    float smoothness;
};

struct Light
{
	// ����Ʈ�� ��ġ
    float3 pos;
    
    // ����Ʈ�� ����
    int MaxLightCount;

	// ����Ʈ�� ����
    float4 dir;

	// ����Ʈ�� ����
    int lightType;

	// ����Ʈ ����Ʈ�� ���, �󸶳� �а� �������̳ĸ� ������.
    float spot_angle;

	// ����Ʈ�� �ݿ��Ǵ� �Ÿ��� ��Ÿ��
    float range;

	// ����Ʈ�� �󸶳� ������
    float intencity;

	// ���� ����
    float4 LightColor;
};

struct ChunckLightData
{
    Light lights[32];
};

cbuffer cbLightInfo : register(b2)
{
    // �¾��
    DirectionLight dirLight;
    matrix DirLightProjection[3];
    matrix DirLightView[3];
    float4 DirLightPos[3];
    
    // ����ƽ �������� ���� �߰� ���.
    float4 ChunckStart; // ���� �ε����� ���� ûũ ������.
    uint4 ChunckCount; // ûũ�� ����
}

cbuffer cbMaterial : register(b3)
{
    float4 baseColor;
    float4 ambientColor;
    float4 emissiveColor;
    float metalicFactor;
    float smoothness;
    float bumpScaling;
    float extra;
    
    float TilingX;
    float TilingY;
    float TilingOffsetX;
    float TilingOffsetY;
}

cbuffer cbHitFlash : register(b5)
{
    float4 HitFlash; // x: blend amount, yzw: flash/status color
}

Texture2D PBR_Tex[5] : register(t0);
Texture2D LightShadowMap[3] : register(t5); // t5 t6 t7 (t8 ?? �ʿ��Ѱ�?)
TextureCube EnvironmentCubeMap : register(t9); // t9 : ȯ���
StructuredBuffer<ChunckLightData> ChunckStaticLightArr : register(t10); // t10 : Static Light Structured Buffer

/*
t0 : color
t1 : normal
t2 : AO
t3 : metalic
t4 : Roughness
*/

// Ambient Occulusion
SamplerState StaticSampler : register(s0);

int GetChunkIndexFromPosition(float3 pos)
{
    float3 cspos = pos - ChunckStart.xyz;
    cspos /= CHUNCK_WIDTH;
    cspos = floor(cspos);
    int index = int(cspos.z + cspos.y * ChunckCount.z + cspos.x * ChunckCount.z * ChunckCount.y);
    return index;
}

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

// Base in LearnOpenGL PBR function modified
float Dfunction(float3 N, float3 H, float a, float3 V, float shininess)
{
    // �� ������ �����ϸ鼭 �þ����� �߰��ϴ� Ʈ��
    float NdotH = max(dot(N, H), 0.0);
    // �ü��� ����������(�ָ� ������) ���̶���Ʈ�� ������ ������ �۶߸�
    float stretching = 1.0 / (max(dot(N, V), 0.0) + 0.1);
    float spec = pow(NdotH, shininess / stretching); // �ŵ����� ������ ���缭 �۶߸�
    
    float a2 = a * a;
    //float NdotH = max(dot(N, H), 0);
    //float NdotH2 = NdotH * NdotH;
    float NdotH2 = spec * spec;
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

float3 BRDF_cookToorrance_Direct(float3 Pos, float3 Wi, float3 ViewDir,
    float3 N, float Roughness, float3 Frenel)
{
    float3 Half = normalize(Wi + ViewDir);
    float D = Dfunction(N, Half, Roughness, ViewDir, 1 - Roughness);
    float G = Gfunction(N, ViewDir, Wi, K_Direct(Roughness));
    float denom = 4 * max(dot(ViewDir, N), 0.0) * max(dot(Wi, N), 0.0) + 1e-5;
    return D * Frenel * G / denom;
}

float3 BRDF_cookToorrance_IBL(float3 Pos, float3 Wi, float3 ViewDir,
    float3 N, float Roughness, float3 Frenel)
{
    float3 Half = normalize(Wi + ViewDir);
    float D = Dfunction(N, Half, Roughness, ViewDir, 1 - Roughness);
    float G = Gfunction(N, ViewDir, Wi, K_IBL(Roughness));
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
};

VS_OUTPUT VSMain(VS_INPUT input)
{
    VS_OUTPUT output;
    output.positionW = mul(float4(input.position, 1.0f), gWorld);
    output.position = mul(output.positionW, gView);
    output.normalW = mul(input.normal, (float3x3) gWorld);
    float4 cameraPosW = mul(Camera_Position, gWorld);
    output.uv.x = TilingX * (input.uv.x + TilingOffsetX);
    output.uv.y = TilingY * (input.uv.y + TilingOffsetY);
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
    output.ViewDir = normalize(output.positionW.xyz - Camera_Position.xyz);
    
    return (output);
}

float4 DefaultPS(VS_OUTPUT input)
{
    float3 Color = PBR_Tex[0].Sample(StaticSampler, input.uv);
    float3 TBNnormal = PBR_Tex[1].Sample(StaticSampler, input.uv);
    TBNnormal = TBNnormal.xzy;
    float AmbientOculusion = PBR_Tex[2].Sample(StaticSampler, input.uv).r;
    float Metalic = PBR_Tex[3].Sample(StaticSampler, input.uv).r;
    float Roughness = PBR_Tex[4].Sample(StaticSampler, input.uv).r;
    //float3 Metalic = float3(0.5f, 0.5f, 0.5f);
    //float Roughness = 0.5f;
    
    const float3 LightPos = float3(0.0f, 1.0f, 0.0f);
    const float LightIntencity = 100.0f;
    const float LightRange = 30.0f;
    const float4 tempLightColor = dirLight.gLightColor;
    const float4 tempAmbientColor = float4(0.01f, 0.01f, 0.01f, 1.0f); // Ambient_Color
    
    float3x3 invTBN = transpose(float3x3(input.T, input.B, input.N));
    
    // get from world space : normal, location vector
    float3 normalW = normalize(mul(TBNnormal, invTBN));
    float3 positionW = input.positionW.xyz;
    
    //attenuation
    float distance = length(LightPos - positionW);
    float attenuation = 1.0f;
    //4.0 * LightIntencity / (2.0 / LightRange + 5.0 * distance / LightRange + 6.0 * LightIntencity * (distance * distance) / LightRange);
    //attenuation = 2.0 * (Sigmoid(attenuation) - 0.5);
    
    // Ambient
    float3 ambient = tempAmbientColor.rgb;
    
    // Diffuse
    float3 lightVector = -dirLight.gLightDirection;
    float diffuse = saturate(dot(normalW, lightVector)); // dot normal & light direction 
    float3 diffuseColor = baseColor.xyz * tempLightColor.rgb * diffuse;
    
    // Specular
    float3 reflectDir = reflect(lightVector, normalW);
    float3 viewDir = input.ViewDir;
    float specular = pow(saturate(dot(viewDir, reflectDir)), 5.0f); // dot half vector & normal
    float3 specularColor = Metalic * tempLightColor.rgb * specular;
    
    // Result Color
    float3 finalColor = attenuation * ambient + attenuation * (Color * diffuseColor) + attenuation * (specularColor * Color);
    return float4(finalColor, 1.0f);
}

float4 PBRonOneLightRay(float3 invviewDir, float3 wi, float3 normalW, float3 Frenel0, float Roughness, float3 MatColor, float3 LightColor, float3 hitpos)
{
    float3 H = normalize(invviewDir + wi);
    float3 radiance = LightColor;
        // Cook-Torrance BRDF
    float3 F = Ffunction(max(dot(H, wi), 0), Frenel0);
        // scale light by NdotL
    float NdotL = max(dot(normalW, wi), 0.0);
        // add to outgoing radiance Lo
    float pbrRough = max(Roughness, 0.02f);
    return float4((BRDF_lambert(MatColor) + BRDF_cookToorrance_Direct(hitpos, wi, invviewDir, normalW, pbrRough, F)) * radiance * NdotL, 1);
}

float4 PBRonOneLightRay_IBL(float3 invviewDir, float3 wi, float3 normalW, float3 Frenel0, float Roughness, float3 MatColor, float3 LightColor, float3 hitpos)
{
    float3 H = normalize(invviewDir + wi);
    float3 radiance = LightColor;
        // Cook-Torrance BRDF
    float3 F = Ffunction(max(dot(H, wi), 0), Frenel0);
        // scale light by NdotL
    float NdotL = max(dot(normalW, wi), 0.0);
        // add to outgoing radiance Lo
    float pbrRough = max(Roughness, 0.02f);
    return float4((BRDF_lambert(MatColor) + BRDF_cookToorrance_IBL(hitpos, wi, invviewDir, normalW, pbrRough, F)) * radiance * NdotL, 1);
}

float CosAttenuation(float x)
{
    return 0.5 * cos(3.141592 * x) + 0.5;
}

float4 PBRPS(VS_OUTPUT input, float isShadow)
{
    float3 Color = PBR_Tex[0].Sample(StaticSampler, input.uv) * baseColor;
    float hitFlashAmount = saturate(HitFlash.x);
    float hitFlashGlow = saturate((HitFlash.x - 0.60f) / 0.75f);
    Color = lerp(Color, HitFlash.yzw, hitFlashAmount);
    Color += HitFlash.yzw * hitFlashGlow * 0.85f;
    float3 TBNnormal = PBR_Tex[1].Sample(StaticSampler, input.uv);
    TBNnormal = TBNnormal.xyz;
    TBNnormal = 2.0 * (TBNnormal - 0.5);
    float AmbientOculusion = PBR_Tex[2].Sample(StaticSampler, input.uv).r;
    float Metalic = PBR_Tex[3].Sample(StaticSampler, input.uv).r;
    float Roughness = max(min(smoothness + PBR_Tex[4].SampleLevel(StaticSampler, input.uv, 0).r, 1.0), 0.02f);
    
    float fValue = 0.2f;
    //(1.0f - 0.02f) * Metalic + 0.02f;
    float3 Frenel0 = float3(fValue, fValue, fValue);
    Frenel0 = lerp(Frenel0, Color, Metalic);
    
    float3 albedo = pow(Color, float3(2.2, 2.2, 2.2));
    float3x3 TBN = float3x3(input.T, input.B, input.N);
    float3x3 invTBN = transpose(TBN);
    float3 normalW = normalize(mul(invTBN, TBNnormal));
    //normalW = input.normalW;
    
    //PBR operation
    float sum = 0;
    int pointLightCount = 0;
    
    ChunckLightData cld = ChunckStaticLightArr[GetChunkIndexFromPosition(input.positionW.xyz)];
    int AdditionLightCount = cld.lights[0].MaxLightCount;
    float LightCalculCount = 0;
    float3 Lo = float3(0.0, 0, 0);
    float3 diffuseColor = 0;
    float3 viewDir = normalize(input.positionW.xyz - Camera_Position.xyz);
    float3 invviewDir = -viewDir;
    
    // Light Calculation
    {
        //DirLight
        float3 wi = normalize(-dirLight.gLightDirection);
        Lo += isShadow * PBRonOneLightRay(invviewDir, wi, normalW, Frenel0, Roughness, Color.xyz, dirLight.gLightColor.xyz, input.positionW.xyz);
        LightCalculCount += 1;
        
        //Environment Light
        wi = reflect(viewDir, normalW);
        float Envlod = floor(10 * Roughness);
        float EnvRate = 10 * Roughness - floor(10 * Roughness);
        float3 EnvColor0 = EnvironmentCubeMap.SampleLevel(StaticSampler, wi, Envlod).xyz;
        float3 EnvColor1 = EnvironmentCubeMap.SampleLevel(StaticSampler, wi, Envlod + 1).xyz;
        float3 EnvColor = EnvRate * EnvColor1 + (1 - EnvColor1) * EnvColor0;
        Lo += isShadow * PBRonOneLightRay_IBL(invviewDir, wi, normalW, Frenel0, Roughness, Color.xyz, EnvColor, input.positionW.xyz);
        LightCalculCount += 1;
        
        for (int i = 0; i < AdditionLightCount; ++i)
        {
            Light l = cld.lights[i];
            float3 LightVector = input.positionW.xyz - l.pos;
            float rate = 1 - (length(LightVector) / l.range);
            if (l.lightType == 0) // SpotLight
            {
                LightVector = normalize(LightVector);
                if (dot(LightVector, l.dir.xyz) > cos(0.5 * 3.141592 * l.spot_angle / 180) && rate > 0)
                {
                    wi = -LightVector;
                    float3 Radiance = l.LightColor * l.intencity * CosAttenuation(1 - rate);
                    Lo += PBRonOneLightRay(invviewDir, wi, normalW, Frenel0, Roughness, Color.xyz, Radiance, input.positionW.xyz);
                    LightCalculCount += 1;
                    //return float4(1, 1, 1, 1);
                }
            }
            else if (l.lightType == 2) // PointLight
            {
                if (rate > 0)
                {
                    wi = normalize(-LightVector);
                    float3 Radiance = l.LightColor * l.intencity * CosAttenuation(1 - rate);
                    Lo += PBRonOneLightRay(invviewDir, wi, normalW, Frenel0, Roughness, Color.xyz, Radiance, input.positionW.xyz);
                    LightCalculCount += 1;
                    //return float4(1, 1, 1, 1);
                }
            }
        }
    }
    //Lo /= LightCalculCount;
    
    //diffuseColor = saturate(diffuseColor);
    float3 ambient = float3(0.03, 0.03, 0.03) * albedo * AmbientOculusion;
    float3 color = ambient + Lo;

    float exposure = 1.0; // 1.0���� ũ�� �� ����
    color = 1.0 - exp(-color * exposure); // exponential tone mapping
    // HDR tonemapping
    color = color / (color + float3(0.5f, 0.5f, 0.5f));
    // gamma correct
    color = pow(color, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
    
    // �̰� �������� �������� �ƴϴ�.
    // ������ �� ����ŧ���� �߰��ϴ� ���� �� ���Ⱑ ������
#ifdef ADD_PONG_SPECULAR
        float3 reflectDir = reflect(normalize(dirLight.gLightDirection), normalW);
        float specular = pow(saturate(dot(invviewDir, reflectDir)), 10.0f - 10*Roughness); // dot half vector & normal
        float3 specularColor = dirLight.gLightColor.xyz * specular * 0.25f;
        color = saturate(color + specularColor * isShadow);
#else
    //color = saturate(color + specular);
#endif
    
#ifdef DEBUG_ENVIRONMENTLIGHT
    float3 wi = reflect(normalize(viewDir), normalW);
    float3 EnvColor = EnvironmentCubeMap.SampleLevel(StaticSampler, wi, 0).xyz;
    float reflectRate = 0.5;
    return float4((1 - reflectRate) * color + reflectRate * EnvColor, 1.0);
#endif
    
#ifdef DEBUG_NORMAL
    return float4(normalW, 1.0);
#else
    return float4(color, 1.0);
#endif
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

 //�ȼ� ���̴��� �����Ѵ�.
float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
#ifdef DEBUG_NORMAL
    return PBRPS(input);
#endif
    const float Dir_ambient = 0.2f;
    for (int i = 0; i < 3; ++i)
    {
        // Compute pixel position in light space.
        float4 vLightSpacePos = input.positionW;
        vLightSpacePos = mul(vLightSpacePos, DirLightView[i]);
        vLightSpacePos = mul(vLightSpacePos, DirLightProjection[i]);
        vLightSpacePos.xyz /= vLightSpacePos.w;
        bool b = vLightSpacePos.x == clamp(vLightSpacePos.x, -1, 1);
        b = b && vLightSpacePos.y == clamp(vLightSpacePos.y, -1, 1);
        if (b)
        {
        //vLightSpacePos.xy = vLightSpacePos.xy * 0.5f + 0.5f;

            float2 vShadowTexCoord = 0.5f * vLightSpacePos.xy + 0.5f;
            vShadowTexCoord.y = 1.0f - vShadowTexCoord.y;
        
        // Depth bias to avoid pixel self-shadowing.
            float vLightSpaceDepth = vLightSpacePos.z - SHADOW_DEPTH_BIAS;

        // Find sub-pixel weights.
            float shadowMapResolution = (i == 0) ? 4096.0f : ((i == 1) ? 2048.0f : 1024.0f);
            float2 vShadowMapDims = float2(shadowMapResolution, shadowMapResolution); // need to keep in sync with .cpp file
            float4 vSubPixelCoords = float4(1.0f, 1.0f, 1.0f, 1.0f);
            vSubPixelCoords.xy = frac(vShadowMapDims * vShadowTexCoord);
            vSubPixelCoords.zw = 1.0f - vSubPixelCoords.xy;
            float4 vBilinearWeights = vSubPixelCoords.zxzx * vSubPixelCoords.wwyy;
        
            float2 vTexelUnits = 1.0f / vShadowMapDims;
            float4 vShadowDepths;
            vShadowDepths.x = LightShadowMap[i].Sample(StaticSampler, vShadowTexCoord);
            vShadowDepths.y = LightShadowMap[i].Sample(StaticSampler, vShadowTexCoord + float2(vTexelUnits.x, 0.0f));
            vShadowDepths.z = LightShadowMap[i].Sample(StaticSampler, vShadowTexCoord + float2(0.0f, vTexelUnits.y));
            vShadowDepths.w = LightShadowMap[i].Sample(StaticSampler, vShadowTexCoord + vTexelUnits);
            
        // What weighted fraction of the 4 samples are nearer to the light than this pixel?
            float4 vShadowTests = (vShadowDepths >= vLightSpaceDepth) ? 1.0f : 0.0f;
        //dot(vBilinearWeights, vShadowTests);
            float f = (vShadowTests.x + vShadowTests.y + vShadowTests.z + vShadowTests.w) * 0.25f;
            float4 Color = PBRPS(input, f);
            return float4(Color.xyz, 1);
        }
    }
    
    return float4(0, 0, 0, 1);
}

////////////////////////////////////////////////
// With Terrain Tesslation
///////////////////////////////////////////////

Texture2D Terrain_HeightMap : register(t6);

struct Terrain_Tess_VS_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float3 tangent : TANGENT;
};

struct Terrain_Tess_VS_OUTPUT
{
    float3 positionW : POSITION;
    float2 uv : TEXCOORD0;
    float3 normalW : NORMAL;
};

Terrain_Tess_VS_OUTPUT TessTerrainVSMain(Terrain_Tess_VS_INPUT input)
{
    Terrain_Tess_VS_OUTPUT output;
    output.positionW = mul(float4(input.position, 1.0f), gWorld);
    output.normalW = mul(input.normal, (float3x3) gWorld);
    output.uv.x = TilingX * (input.uv.x + TilingOffsetX);
    output.uv.y = TilingY * (input.uv.y + TilingOffsetY);
    output.normalW = normalize(mul(input.normal, (float3x3) gWorld));
    return (output);
}

struct PatchTess
{
    float EdgeTess[4] : SV_TessFactor;
    float InsideTess[2] : SV_InsideTessFactor;
};

/*----------------CONSTANT HULL SHADER------------------------------------------*/
PatchTess ConstantHS(InputPatch<Terrain_Tess_VS_OUTPUT, 4> patch, uint patchID : SV_PrimitiveID)
{
    PatchTess patchTess;
    float3 objectCenter = patch[0].positionW + patch[1].positionW + patch[2].positionW + patch[3].positionW;
    objectCenter /= 4.0f;
    objectCenter.y = Camera_Position.y;
    
    float distanceToCamera = distance(Camera_Position.xyz, objectCenter);
    float f = 1.0 + 31.0 * exp(-distanceToCamera / 100.0);
    float tessFactor = floor(f);

    patchTess.EdgeTess[0] = tessFactor;
    patchTess.EdgeTess[1] = tessFactor;
    patchTess.EdgeTess[2] = tessFactor;
    patchTess.EdgeTess[3] = tessFactor;
    patchTess.InsideTess[0] = tessFactor;
    patchTess.InsideTess[1] = tessFactor;
    
    return patchTess;
}

struct HullOut
{
    float3 positionW : POSITION;
    float2 uv : TEXCOORD0;
    float3 normalW : NORMAL;
};

/*----------------HULL SHADER------------------------------------------*/
[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64.0f)]
HullOut HS_Main(InputPatch<Terrain_Tess_VS_OUTPUT, 4> p, uint i : SV_OutputControlPointID, uint patchId : SV_PrimitiveID)
{
    HullOut hout;
    hout.positionW = p[i].positionW;
    hout.uv = p[i].uv;
    hout.normalW = p[i].normalW;
    return hout;
}

struct DomainOut
{
    float4 position : SV_POSITION;
    float3 positionW : POSITION;
    float2 uv : TEXCOORD0;
    float3 T : TEXCOORD1;
    float3 B : TEXCOORD2;
    float3 normalW : NORMAL;
};

/*----------------DOMAIN SHADER------------------------------------------*/
[domain("quad")]
DomainOut DS_Main(PatchTess patchTess, float2 uv : SV_DomainLocation, const OutputPatch<HullOut, 4> quad)
{
    DomainOut dout;

    float3 v1 = lerp(quad[0].positionW, quad[1].positionW, uv.x);
    float3 v2 = lerp(quad[3].positionW, quad[2].positionW, uv.x);
    float3 p = lerp(v1, v2, uv.y);
    
    float divEdge = patchTess.EdgeTess[0] - 1;
    float dist = distance(quad[0].positionW, quad[1].positionW) / divEdge;
    
    float3 TanV = quad[1].positionW - quad[0].positionW;
    TanV = normalize(TanV) * dist;
    float3 BitV = quad[3].positionW - quad[0].positionW;
    BitV = normalize(BitV) * dist;
    
    float2 uv_v1 = lerp(quad[0].uv, quad[1].uv, uv.x);
    float2 uv_v2 = lerp(quad[3].uv, quad[2].uv, uv.x);
    float2 xuvdelta = (quad[1].uv - quad[0].uv);
    float2 yuvdelta = (quad[3].uv - quad[0].uv);
    float deltaTanUV = (length(xuvdelta) / divEdge) * normalize(xuvdelta);
    float deltaBitUV = (length(yuvdelta) / divEdge) * normalize(yuvdelta);
    dout.uv = lerp(uv_v1, uv_v2, uv.y);

    float heightMul = 50.0f;
    float height = Terrain_HeightMap.SampleLevel(StaticSampler, dout.uv, 0).r;
    p.y += height * heightMul;
    float3 tanp = p + TanV + Terrain_HeightMap.SampleLevel(StaticSampler, dout.uv + deltaTanUV, 0).r * heightMul;
    float3 bitp = p + BitV + Terrain_HeightMap.SampleLevel(StaticSampler, dout.uv + deltaBitUV, 0).r * heightMul;
    //Set TBN
   
    dout.T = normalize(tanp - p);
    dout.B = normalize(bitp - p);
    dout.normalW = cross(dout.T, dout.B);
    dout.positionW = p;
    dout.position = mul(float4(p, 1.0f), gView);

    return dout;
}

float4 TessTerrainPSMain(DomainOut input) : SV_TARGET
{
    //return PBR_Tex[0].Sample(StaticSampler, input.uv);
    VS_OUTPUT vsout;
    vsout.position = input.position;
    vsout.positionW = float4(input.positionW, 1);
    vsout.normalW = input.normalW;
    vsout.N = input.normalW;
    vsout.T = input.T;
    vsout.B = input.B;
    vsout.uv = input.uv;
    vsout.ViewDir = Camera_Position.xyz - input.positionW;
    
    //float depth = LightShadowMap.Sample(StaticSampler, vShadowTexCoord).r;
    if (distance(Camera_Position.xyz, input.positionW) > 200)
    {
        return PBRPS(vsout, 1);
    }
    else
    {
        // Compute pixel position in light space.
        float4 vLightSpacePos = float4(input.positionW, 1.0f);
        vLightSpacePos = mul(vLightSpacePos, DirLightView[0]);
        vLightSpacePos = mul(vLightSpacePos, DirLightProjection[0]);
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

    // 2x2 percentage closer filtering.
        float2 vTexelUnits = 1.0f / vShadowMapDims;
        float4 vShadowDepths;
        vShadowDepths.x = LightShadowMap[0].Sample(StaticSampler, vShadowTexCoord);
        vShadowDepths.y = LightShadowMap[0].Sample(StaticSampler, vShadowTexCoord + float2(vTexelUnits.x, 0.0f));
        vShadowDepths.z = LightShadowMap[0].Sample(StaticSampler, vShadowTexCoord + float2(0.0f, vTexelUnits.y));
        vShadowDepths.w = LightShadowMap[0].Sample(StaticSampler, vShadowTexCoord + vTexelUnits);

    // What weighted fraction of the 4 samples are nearer to the light than this pixel?
        float4 vShadowTests = (vShadowDepths >= vLightSpaceDepth) ? 1.0f : 0.0f;
        float f = 0.25 * (vShadowTests.x + vShadowTests.y + vShadowTests.z + vShadowTests.w);
        //dot(vBilinearWeights, vShadowTests);
        
        float4 Color = PBRPS(vsout, 1);
        if (f <= 0.001 /*abs(vShadowDepths - vLightSpaceDepth) > SHADOW_DEPTH_BIAS*/)
        {
            //PBR_Tex[0].Sample(StaticSampler, input.uv);
            return float4(Color.xyz * 0.2f, 1);
        }
        return float4((f * Color).xyz, 1);
    }
}