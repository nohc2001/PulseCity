//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS 8

Texture2D material_diffuse : register(t0);
Texture2D material_specular : register(t1);
Texture2D material_normal : register(t2);
SamplerState samplerDiffuse : register(s0);

struct DirLight
{
    float3 direction;

    float3 ambient;
    float3 diffuse;
    float3 specular;
};

struct PointLight
{
    float3 position;
    float intencity;
    float range;
};

struct SpotLight
{
    float3 position;
    float3 direction;

    float intencity;
    float range;

    float cutOff;
    float outerCutOff;

    float3 ambient;
    float3 diffuse;
    float3 specular;
};

//256 limit
cbuffer CONSTANT_BUFFER_DEFAULT : register(b0)
{
    float4x4 model; // 64 byte
    float4x4 view; // 128 byte
    float4x4 project; // 196 byte
    PointLight pointLights; // 216 byte
    float3 CamPos; // 228 byte
}

struct VSInput
{
    float4 Pos : POSITION;
    float3 Normal : NORMAL0;
    float2 TexCoord : TEXCOORD0;
    float3 Tangent : TEXCOORD1;
    float3 Bitangent : TEXCOORD2;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    float3 mFragPos : TEXCOORD2;
    float3 mViewPos : TEXCOORD3;
    float3 mLightPos : TEXCOORD4;
};

PSInput VSMain(VSInput input)
{
    PSInput result = (PSInput) 0;
    float4 p = mul(input.Pos, model);
    result.position = mul(p, view);
    result.position = mul(result.position, project);
    //result.mNormal = normalize(mul(float4(input.Normal, 1.0f), model).xyz);
    //result.color = input.color;
    
    float3 T = normalize(mul(float4(input.Tangent, 1), model).xyz);
    float3 N = normalize(mul(float4(input.Normal, 1), model).xyz);
    float3 B = normalize(mul(float4(input.Bitangent, 1), model).xyz);
    //T = normalize(T - dot(T, N) * N);
    //float3 B = cross(N, T);
    
    float3x3 TBN = transpose(float3x3(T, B, N));
    result.TexCoord = input.TexCoord;
    result.mLightPos = mul(pointLights.position, TBN);
    result.mViewPos = mul(CamPos, TBN);
    result.mFragPos = mul(p.xyz, TBN);
    
    //float3 lightDir = p.xyz - pointLights.position;
    //lightDir = normalize(lightDir);
    
    //result.mViewDir = normalize(p.xyz - CamPos);
    
    return result;
}

float Sigmoid(float x)
{
    return 1.0 / (1.0 + (exp(-x)));
}

float3 CalcPointLight(PointLight light, float3 normal, float3 fragPos, float3 viewDir, float2 TexCoord)
{
    float3 lightDir = normalize(light.position - fragPos);
	// diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
	// specular shading
    float3 reflectDir = reflect(-lightDir, normal);
    
    float material_shininess = 2.0f;
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material_shininess);
	// attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 4.0 * light.intencity / (2.0 / light.range + 5.0 * distance / light.range +
		6.0 * light.intencity * (distance * distance) / light.range);
    attenuation = 2.0 * (Sigmoid(attenuation) - 0.5);

	// combine results
    //float3 ambient = light.ambient * material_ambient.Sample(samplerDiffuse, TexCoord).xyz;
    float3 diffuse = diff * material_diffuse.Sample(samplerDiffuse, TexCoord).xyz;
    //diffuse = material_diffuse.Sample(samplerDiffuse, TexCoord).xyz;
    float3 specular = spec * material_specular.Sample(samplerDiffuse, TexCoord).xyz;
    //ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    return diffuse + specular; //(ambient + diffuse + specular);
}

float4 PSMain(PSInput input) : SV_TARGET
{
    //float3 normal = material_normal.Sample(samplerDiffuse, input.TexCoord).xyz;
    //// transform normal vector to range [-1,1]
    //normal = normalize(2.0f * normal - 1.0f); // this normal is in tangent space
    //// get diffuse color
    //float3 color = material_diffuse.Sample(samplerDiffuse, input.TexCoord).xyz;
    //// ambient
    //float3 ambient = 0.1 * color;
    //// diffuse
    //float3 lightDir = normalize(input.mLightPos - input.mFragPos);
    //float diff = max(dot(lightDir, normal), 0.0);
    //float3 diffuse = diff * color;
    //// specular
    //float3 viewDir = normalize(input.mViewPos - input.mFragPos);
    //float3 reflectDir = reflect(-lightDir, normal);
    //float3 halfwayDir = normalize(lightDir + viewDir);
    //float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);

    //float3 specular = float3(0.2f, 0.2f, 0.2f) * spec; // 0.2 = metailic value
    ////return float4(1.0f, 1.0f, 1.0f, 1.0f); // vertex debuging
    //return float4(ambient + diffuse + specular, 1.0f);
    
    ////return float4(CalcPointLight(pointLights, normal, input.mFragPos, input.mViewPos, input.TexCoord), 1.0f);
    
    //input.normalW = normalize(input.normalW);
    
    //float3 vCameraPosition = float3(input.mViewPos.x, input.mViewPos.y, input.mViewPos.z);
    
    float3 vToCamera = normalize(input.mViewPos - input.mFragPos);
    float4 cColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
    
    //cColor += PointLight(0, input.positionW, input.normalW, vToCamera);
    float3 vToLight = input.mLightPos - input.mFragPos;
    float fDistance = length(vToLight);

    float3 normal = material_normal.Sample(samplerDiffuse, input.TexCoord).xyz;
    normal = normalize(2.0f * normal - 1.0f); // this normal is in tangent space
    
    float fSpecularFactor = 0.0f;
    vToLight /= fDistance;
    float fDiffuseFactor = dot(vToLight, normal);
    float4 specular = material_specular.Sample(samplerDiffuse, input.TexCoord);
    if (fDiffuseFactor > 0.0f)
    {
        if (specular.a != 0.0f)
        {
            float3 vHalf = normalize(vToCamera + vToLight);
            fSpecularFactor = pow(max(dot(vHalf, normal), 0.0f), specular.a);
        }
    }
    float4 diffuse = material_diffuse.Sample(samplerDiffuse, input.TexCoord);
    float fAttenuationFactor = 1.0f / dot( /*gLights[nIndex].m_vAttenuation*/float3(0.3f, 0.3f, 0.3f), float3(1.0f, fDistance, fDistance * fDistance));
    cColor += (((0.01f) + ( /*gLights[nIndex].m_cDiffuse*/float4(1, 1, 1, 1) * fDiffuseFactor * diffuse) + ( /*gLights[nIndex].m_cSpecular*/0.1f * fSpecularFactor * specular)) * fAttenuationFactor);
    //cColor += fDiffuseFactor * diffuse;
    float4 gcGlobalAmbientLight = float4(0.1f, 0.1f, 0.1f, 1.0f);
    cColor += (gcGlobalAmbientLight * /*gMaterial.m_cAmbient*/0.1f);
    cColor.a = diffuse.a;
    float4 color = cColor;
    return color;
}


//PSInput VSMain(VSInput input)
//{
//    PSInput result = (PSInput) 0;
//    float4 p = mul(input.Pos, model);
//    result.position = mul(p, view);
//    result.position = mul(result.position, project);
    
//    float3 T = normalize(mul(float4(input.Tangent, 1), model)).xyz;
//    float3 N = normalize(mul(float4(input.Normal, 1), model)).xyz;
//    T = normalize(T - dot(T, N) * N);
//    float3 B = cross(N, T);
    
//    float3x3 TBN = transpose(float3x3(T, B, N));
    
//    result.TexCoord = input.TexCoord;
//    result.mLightPos = mul(pointLights.position, TBN);
//    result.mViewPos = mul(CamPos, TBN);
//    result.mFragPos = mul(p.xyz, TBN);
    
//    return result;
//}

//float Sigmoid(float x)
//{
//    return 1.0 / (1.0 + (exp(-x)));
//}

//float3 CalcPointLight(PointLight light, float3 normal, float3 fragPos, float3 viewPos, float2 TexCoord, float3 lightPos)
//{
//	// get diffuse color
//    float3 color = material_diffuse.Sample(samplerDiffuse, TexCoord);
//    // ambient
//    float3 ambient = 0.1 * color;
//    // diffuse
//    float3 lightDir = normalize(lightPos - fragPos);
//    float diff = max(dot(lightDir, normal), 0.0);
//    float3 diffuse = diff * color;
//    // specular
//    float3 viewDir = normalize(viewPos - fragPos);
//    float3 reflectDir = reflect(-lightDir, normal);
//    float3 halfwayDir = normalize(lightDir + viewDir);
//    float spec = pow(max(dot(normal, halfwayDir), 0.0f), 32.0f);

//    float3 specular = 0.2f * spec; // metailic
//    return ambient + diffuse + specular;
//}

//float4 PSMain(PSInput input) : SV_TARGET
//{
//    float3 normal = material_normal.Sample(samplerDiffuse, input.TexCoord).xyz;
//    // transform normal vector to range [-1,1]
//    normal = normalize(2.0f * normal - 1.0f); // this normal is in tangent space
    
//    return float4(CalcPointLight(pointLights, normal, input.mFragPos, input.mViewPos, input.TexCoord, input.mLightPos), 1.0f);
//}
