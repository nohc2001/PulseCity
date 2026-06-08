//#define DEBUG_NORMAL
//#define ADD_PONG_SPECULAR
//#define DEBUG_ENVIRONMENTLIGHT

// ûũ ũ�Ⱑ �޶����� �ٲپ�� ��.
#define CHUNCK_WIDTH 50

cbuffer cbCameraInfo : register(b0)
{
    //matrix gProjection;
    matrix gView;
    float4 Camera_Position;
};

cbuffer cbBoneToLocalWorldInfo : register(b1)
{
    matrix ToLocalMatrixs[128];
};

cbuffer cbWorldInfo : register(b2)
{
    //matrix gWorld;
    matrix ToWorldMatrixs[128];
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

Texture2D PBR_Tex[5] : register(t0);
Texture2D LightShadowMap[3] : register(t5); // t5, t6, t7
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

cbuffer cbLightInfo : register(b3)
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

cbuffer cbMaterial : register(b4)
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
    float4 HitFlash; // x: blend amount, yzw: flash color
}

/*
t0 : color
t1 : normal
t2 : AO
t3 : metalic
t4 : Roughness
*/

struct VS_INPUT
{
    float3 position : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float extra : EXTRA;
    uint boneindex0 : BONEINDEX0;
    float weight0 : BONEWEIGHT0;
    uint boneindex1 : BONEINDEX1;
    float weight1 : BONEWEIGHT1;
    uint boneindex2 : BONEINDEX2;
    float weight2 : BONEWEIGHT2;
    uint boneindex3 : BONEINDEX3;
    float weight3 : BONEWEIGHT3;
};

int GetChunkIndexFromPosition(float3 pos)
{
    float3 cspos = pos - ChunckStart.xyz;
    cspos /= CHUNCK_WIDTH;
    cspos = floor(cspos);
    int index = int(cspos.z + cspos.y * ChunckCount.z + cspos.x * ChunckCount.z * ChunckCount.y);
    return index;
}

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

VS_OUTPUT VSMain_RenderShadow(VS_INPUT input)
{
    VS_OUTPUT output;
    float4x4 mtxVertexToBoneWorld = (float4x4) 0.0f;
    matrix w0 = ToWorldMatrixs[input.boneindex0];
    matrix w1 = ToWorldMatrixs[input.boneindex1];
    matrix w2 = ToWorldMatrixs[input.boneindex2];
    matrix w3 = ToWorldMatrixs[input.boneindex3];
    mtxVertexToBoneWorld += input.weight0 * mul(ToLocalMatrixs[input.boneindex0], w0);
    mtxVertexToBoneWorld += input.weight1 * mul(ToLocalMatrixs[input.boneindex1], w1);
    mtxVertexToBoneWorld += input.weight2 * mul(ToLocalMatrixs[input.boneindex2], w2);
    mtxVertexToBoneWorld += input.weight3 * mul(ToLocalMatrixs[input.boneindex3], w3);
    output.position = mul(mul(float4(input.position, 1.0f), mtxVertexToBoneWorld), gView);
    return output;
}

VS_OUTPUT VSMain(VS_INPUT input)
{
    VS_OUTPUT output;
    float4x4 mtxVertexToBoneWorld = (float4x4) 0.0f;
    matrix w0 = ToWorldMatrixs[input.boneindex0];
    matrix w1 = ToWorldMatrixs[input.boneindex1];
    matrix w2 = ToWorldMatrixs[input.boneindex2];
    matrix w3 = ToWorldMatrixs[input.boneindex3];
    mtxVertexToBoneWorld += input.weight0 * mul(ToLocalMatrixs[input.boneindex0], w0);
    mtxVertexToBoneWorld += input.weight1 * mul(ToLocalMatrixs[input.boneindex1], w1);
    mtxVertexToBoneWorld += input.weight2 * mul(ToLocalMatrixs[input.boneindex2], w2);
    mtxVertexToBoneWorld += input.weight3 * mul(ToLocalMatrixs[input.boneindex3], w3);
    output.positionW = mul(float4(input.position, 1.0f), mtxVertexToBoneWorld);
    output.normalW = mul(input.normal, (float3x3) mtxVertexToBoneWorld).xyz;
    output.T = mul(input.tangent, (float3x3) mtxVertexToBoneWorld).xyz;
    float3 bitangent = cross(input.normal, input.tangent);
    output.B = mul(bitangent, (float3x3) mtxVertexToBoneWorld).xyz;
    output.N = output.normalW;
    output.uv.x = TilingX * (input.uv.x + TilingOffsetX);
    output.uv.y = TilingY * (input.uv.y + TilingOffsetY);
    output.ViewDir = normalize(Camera_Position.xyz - output.positionW.xyz);
    output.position = mul(output.positionW, gView);
    return (output);
}

//Base in Pong Shadeing
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
    float Roughness = max(min(0.5f + PBR_Tex[4].Sample(StaticSampler, input.uv).r, 1.0), 0.02f);
    
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

float4 OuterPBR(VS_OUTPUT input)
{
    float3 Color = baseColor * PBR_Tex[0].Sample(StaticSampler, input.uv);
    
    float AmbientOculusion = PBR_Tex[2].Sample(StaticSampler, input.uv).r;
    float Metalic = metalicFactor * PBR_Tex[3].Sample(StaticSampler, input.uv).r;
    float Roughness = PBR_Tex[4].Sample(StaticSampler, input.uv).r;
    
    float3 TBNnormal = PBR_Tex[1].Sample(StaticSampler, input.uv);
    TBNnormal = TBNnormal.xyz;
    TBNnormal = 2.0 * (TBNnormal - 0.5);
    float3x3 invTBN = transpose(float3x3(input.T, input.B, input.N));
    float3 normalW = normalize(mul(TBNnormal, invTBN));

    float3 N = normalW;
    float3 V = input.ViewDir;
    float3 color = float3(0, 0, 0);
    
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
    
    //directional Light
    {
        DirectionLight p = dirLight;
        float3 L = normalize(-p.gLightDirection);
        float3 H = normalize(V + L);

        float3 F0 = float3(0.04, 0.04, 0.04); // default for dielectrics
        F0 = lerp(F0, Color, Metalic);

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, Roughness);
        float G = GeometrySmith(N, V, L, Roughness);
        float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

        float3 numerator = NDF * G * F;
        float denominator = 8.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;
        float3 specular = numerator / denominator;

        float3 kS = F;
        float3 kD = 1.0 - kS;
        kD *= 1.0 - Metalic;

        float NdotL = max(dot(N, L), 0.0);
        float3 irradiance = float4(1, 1, 1, 1) * NdotL;

        float3 diffuse = kD * Color / 3.141592f;
        color += (diffuse + specular) * irradiance;
    }
    
    // Apply ambient occlusion
    color = color * AmbientOculusion;

    // Gamma correction
    color = pow(color, 1.0 / 2.2);
    color = color * 0.9 + Color * 0.1;
    
    return float4(color, 1.0);
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
