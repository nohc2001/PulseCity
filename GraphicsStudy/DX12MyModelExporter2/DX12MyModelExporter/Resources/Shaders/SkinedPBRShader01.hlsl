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
    matrix gWorld;
    matrix ToWorldMatrixs[127];
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

cbuffer cbLightInfo : register(b3)
{
    DirectionLight dirLight;
    PointLight pointLightArr[8];
}

Texture2D PBR_Tex[5] : register(t0);
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
    float3 bitangent : BINORMAL;
    uint boneindex0 : BONEINDEX0;
    float weight0 : BONEWEIGHT0;
    uint boneindex1 : BONEINDEX1;
    float weight1 : BONEWEIGHT1;
    uint boneindex2 : BONEINDEX2;
    float weight2 : BONEWEIGHT2;
    uint boneindex3 : BONEINDEX3;
    float weight3 : BONEWEIGHT3;
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
    float4 v0, v1, v2, v3;
    
    // Bone Matrix Calculation
    v0 = mul(output.positionW, ToLocalMatrixs[input.boneindex0]);
    v1 = mul(output.positionW, ToLocalMatrixs[input.boneindex1]);
    v2 = mul(output.positionW, ToLocalMatrixs[input.boneindex2]);
    v3 = mul(output.positionW, ToLocalMatrixs[input.boneindex3]);
    v0 = mul(v0, ToWorldMatrixs[input.boneindex0]);
    v1 = mul(v1, ToWorldMatrixs[input.boneindex1]);
    v2 = mul(v2, ToWorldMatrixs[input.boneindex2]);
    v3 = mul(v3, ToWorldMatrixs[input.boneindex3]);
    output.positionW = (v0 * input.weight0 + v1 * input.weight1 
        + v2 * input.weight2 + v3 * input.weight3) /
        (input.weight0 + input.weight1 + input.weight2 + input.weight3);
    output.positionW.w = 1.0f;
    
    output.position = mul(output.positionW, gView);
    output.normalW = mul(input.normal, (float3x3) gWorld);
    float4 cameraPosW = mul(Camera_Position, gWorld);
    output.uv = input.uv;
    
    float3 T = normalize(mul(input.tangent, (float3x3) gWorld));
    float3 B = normalize(mul(input.bitangent, (float3x3) gWorld));
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
    const float4 tempLightColor = float4(1.0f, 1.0f, 1.0f, 1.0f); // Light_Color
    const float4 tempAmbientColor = float4(0.01f, 0.01f, 0.01f, 1.0f); // Ambient_Color
    
    float3x3 invTBN = transpose(float3x3(input.T, input.B, input.N));
    
    // get from world space : normal, location vector
    float3 normalW = normalize(mul(TBNnormal, invTBN));
    float3 positionW = input.positionW.xyz;
    
    //attenuation
    float distance = length(LightPos - positionW);
    float attenuation = 4.0 * LightIntencity / (2.0 / LightRange + 5.0 * distance / LightRange +
		6.0 * LightIntencity * (distance * distance) / LightRange);
    attenuation = 2.0 * (Sigmoid(attenuation) - 0.5);
    
    // Ambient
    float3 ambient = tempAmbientColor.rgb;
    
    // Diffuse
    float3 lightVector = normalize(LightPos - positionW);
    float diffuse = saturate(dot(normalW, lightVector)); // dot normal & light direction 
    float3 diffuseColor = tempLightColor.rgb * diffuse;
    
    // Specular
    float3 reflectDir = reflect(lightVector, normalW);
    float3 viewDir = input.ViewDir;
    float specular = pow(saturate(dot(viewDir, reflectDir)), 5.0f); // dot half vector & normal
    float3 specularColor = Metalic * tempLightColor.rgb * specular;
    
    // Result Color
    float3 finalColor = attenuation * ambient + attenuation * (diffuseColor * Color) + attenuation * (specularColor * Color);
    return float4(finalColor, 1.0f);
}

float4 PBRPS(VS_OUTPUT input)
{
    float3 Color = PBR_Tex[0].Sample(StaticSampler, input.uv);
    float3 TBNnormal = PBR_Tex[1].Sample(StaticSampler, input.uv);
    TBNnormal = TBNnormal.xzy;
    TBNnormal = 2.0 * (TBNnormal - 0.5);
    float AmbientOculusion = PBR_Tex[2].Sample(StaticSampler, input.uv).r;
    float Metalic = PBR_Tex[3].Sample(StaticSampler, input.uv).r;
    float Roughness = PBR_Tex[4].Sample(StaticSampler, input.uv).r;
    
    float3 Frenel0 = float3(0.02, 0.02, 0.02);
    Frenel0 = lerp(Frenel0, Color, Metalic);
    
    float3 albedo = pow(Color, float3(2.2, 2.2, 2.2));
    float3x3 TBN = float3x3(input.T, input.B, input.N);
    float3x3 invTBN = transpose(TBN);
    float3 normalW = normalize(mul(TBNnormal, invTBN));
    
    //PBR operation
    float sum = 0;
    float dW = 1.0 / 8;
    float3 Lo = float3(0.0, 0, 0);
    float3 diffuseColor = 0;
    for (int i = 0; i < 8; ++i)
    {
        PointLight p = pointLightArr[i];
        float3 wi = normalize(p.LightPos - input.positionW.xyz);
        float3 H = normalize(input.ViewDir.xyz + wi);
        float distance = length(p.LightPos - input.positionW.xyz);
        float attenuation = 1.0f / distance; //1.0 / (distance * distance);
        
        // Diffuse
        float3 lightVector = normalize(p.LightPos - input.positionW.xyz);
        float diffuse = saturate(dot(normalW, lightVector)); // dot normal & light direction 
        diffuseColor += p.LightColor * diffuse * albedo;
        
        float3 radiance = diffuse * p.LightIntencity * attenuation * p.LightColor;
        
        // Cook-Torrance BRDF
        float F = Ffunction(dot(H, wi), Frenel0);

        // scale light by NdotL
        float NdotL = max(dot(normalW, wi), 0.0);

        // add to outgoing radiance Lo
        Lo += dW * (BRDF_cookToorrance(input.positionW.xyz, wi, input.ViewDir.xyz, normalW, Roughness, F)) * radiance * NdotL;
        
        
    }
    //diffuseColor = saturate(diffuseColor);
    float3 ambient = float3(0.03, 0.03, 0.03) * albedo * AmbientOculusion;
    float3 color = ambient + Lo;

    // HDR tonemapping
    color = color / (color + float3(1.0, 1.0, 1.0));
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

float4 OuterPBR(VS_OUTPUT input)
{
    float3 Color = PBR_Tex[0].Sample(StaticSampler, input.uv);
    float3 TBNnormal = PBR_Tex[1].Sample(StaticSampler, input.uv);
    TBNnormal = TBNnormal.xzy;
    TBNnormal = 2.0 * (TBNnormal - 0.5);
    float AmbientOculusion = PBR_Tex[2].Sample(StaticSampler, input.uv).r;
    float Metalic = PBR_Tex[3].Sample(StaticSampler, input.uv).r;
    float Roughness = PBR_Tex[4].Sample(StaticSampler, input.uv).r;
    
    float3x3 invTBN = transpose(float3x3(input.T, input.B, input.N));
    float3 normalW = normalize(mul(TBNnormal, invTBN));

    float3 N = normalW;
    float3 V = input.ViewDir;
    float3 color = float3(0, 0, 0);
    
    for (int i = 0; i < 8; ++i)
    {
        PointLight p = pointLightArr[i];
        float3 L = normalize(p.LightPos - input.positionW.xyz);
        float3 H = normalize(V + L);

        float3 F0 = float3(0.04, 0.04, 0.04); // default for dielectrics
        F0 = lerp(F0, Color, Metalic);

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, Roughness);
        float G = GeometrySmith(N, V, L, Roughness);
        float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

        float3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;
        float3 specular = numerator / denominator;

        float3 kS = F;
        float3 kD = 1.0 - kS;
        kD *= 1.0 - Metalic;

        float NdotL = max(dot(N, L), 0.0);
        float3 irradiance = p.LightColor * NdotL;

        float3 diffuse = kD * Color / 3.141592f;
        color += (diffuse + specular) * irradiance;
    }
    
    // Apply ambient occlusion
    color = color * AmbientOculusion;

    // Gamma correction
    color = pow(color, 1.0 / 2.2);

    return float4(color, 1.0);
}

 //ÇÈ¼¿ ¼ÎÀÌ´õ¸¦ Á¤ÀÇÇÑ´Ù.
float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
    return OuterPBR(input);
    
    /*
    float3 finalColor = float3(0, 0, 0);
    for (int i = 0; i < 8; ++i)
    {
        PointLight p = pointLightArr[i];
    
        //attenuation
        float distance = length(p.LightPos - input.positionW.xyz);
        float attenuation = 4.0 * p.LightIntencity / (2.0 / p.LightRange + 5.0 * distance / p.LightRange +
		6.0 * p.LightIntencity * (distance * distance) / p.LightRange);
        attenuation = 2.0 * (Sigmoid(attenuation) - 0.5);
    
        // Diffuse
        //float3 LightDirection = normalize(positionW - p.LightPos);
        //float3 lightVector = -normalize(LightDirection);
        float diffuse = saturate(dot(TBNnormal, input.TBNPointLightDir)); // dot normal & light direction 
        float3 diffuseColor = p.LightColor.rgb * Color * diffuse;
    
        // Specular
        float3 reflectDir = reflect(input.TBNPointLightDir, TBNnormal);
        float3 viewDir = -input.TBNViewDir;
        float specular = pow(saturate(dot(viewDir, reflectDir)), 32.0f); // dot half vector & normal
        float3 specularColor = Metalic * p.LightColor.rgb * specular;
    
        // Result Color
        float3 fcolor = attenuation * diffuseColor + attenuation * specularColor;
        finalColor += fcolor;
        finalColor = saturate(finalColor);
    }
    
    // Diffuse
    float3 lightVector = -input.TBNDirLightDir;
    float3 diffuse = saturate(dot(TBNnormal, lightVector)); // dot normal & light direction 
    float3 diffuseColor = float3(1.0f, 0.95f, 0.90f) * Color * diffuse;
    
    float3 reflectDir = reflect(input.TBNDirLightDir, TBNnormal);
    float3 viewDir = -input.TBNViewDir;
    float specular = pow(saturate(dot(viewDir, reflectDir)), 32.0f); // dot half vector & normal
    float3 specularColor = Metalic * dirLight.gLightColor.rgb * specular;
    float3 directionLightColor = saturate(diffuseColor + specularColor);
    return float4(finalColor + directionLightColor, 1.0f);
*/
}