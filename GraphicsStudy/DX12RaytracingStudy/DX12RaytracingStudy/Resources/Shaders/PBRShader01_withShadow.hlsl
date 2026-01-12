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

cbuffer cbLightInfo : register(b2)
{
    DirectionLight dirLight;
    PointLight pointLightArr[8];
    
    matrix LightProjection;
    matrix LightView;
    float3 LightPos;
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

Texture2D PBR_Tex[5] : register(t0);
Texture2D LightShadowMap : register(t5);
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
    float3 Color = baseColor * PBR_Tex[0].Sample(StaticSampler, input.uv);
    
    float AmbientOculusion = PBR_Tex[2].Sample(StaticSampler, input.uv).r;
    float Metalic =metalicFactor * PBR_Tex[3].Sample(StaticSampler, input.uv).r;
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

 //ÇÈ¼¿ ¼ÎÀÌ´õ¸¦ Á¤ÀÇÇÑ´Ù.
float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
    //return PBR_Tex[0].Sample(StaticSampler, input.uv);
    
    //float depth = LightShadowMap.Sample(StaticSampler, vShadowTexCoord).r;
    if (distance(Camera_Position, input.positionW) > 200)
    {
        return OuterPBR(input);
    }
    else
    {
        // Compute pixel position in light space.
        float4 vLightSpacePos = input.positionW;
        vLightSpacePos = mul(vLightSpacePos, LightView);
        vLightSpacePos = mul(vLightSpacePos, LightProjection);
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
        
        if (f <= 0.001 /*abs(vShadowDepths - vLightSpaceDepth) > SHADOW_DEPTH_BIAS*/)
        {
            float3 Color = baseColor * PBR_Tex[0].SampleLevel(StaticSampler, input.uv, 1); //PBR_Tex[0].Sample(StaticSampler, input.uv);
            return float4(Color * 0.2f, 1);
        }
    
        return f * OuterPBR(input);
    }
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
        return OuterPBR(vsout);
    }
    else
    {
        // Compute pixel position in light space.
        float4 vLightSpacePos = float4(input.positionW, 1.0f);
        vLightSpacePos = mul(vLightSpacePos, LightView);
        vLightSpacePos = mul(vLightSpacePos, LightProjection);
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
        vShadowDepths.x = LightShadowMap.Sample(StaticSampler, vShadowTexCoord);
        vShadowDepths.y = LightShadowMap.Sample(StaticSampler, vShadowTexCoord + float2(vTexelUnits.x, 0.0f));
        vShadowDepths.z = LightShadowMap.Sample(StaticSampler, vShadowTexCoord + float2(0.0f, vTexelUnits.y));
        vShadowDepths.w = LightShadowMap.Sample(StaticSampler, vShadowTexCoord + vTexelUnits);

    // What weighted fraction of the 4 samples are nearer to the light than this pixel?
        float4 vShadowTests = (vShadowDepths >= vLightSpaceDepth) ? 1.0f : 0.0f;
        float f = 0.25 * (vShadowTests.x + vShadowTests.y + vShadowTests.z + vShadowTests.w);
        //dot(vBilinearWeights, vShadowTests);
        
        if (f <= 0.001 /*abs(vShadowDepths - vLightSpaceDepth) > SHADOW_DEPTH_BIAS*/)
        {
            float3 Color = baseColor * PBR_Tex[0].SampleLevel(StaticSampler, input.uv, 1); //PBR_Tex[0].Sample(StaticSampler, input.uv);
            return float4(Color * 0.2f, 1);
        }
    
        return f * OuterPBR(vsout);
    }
}