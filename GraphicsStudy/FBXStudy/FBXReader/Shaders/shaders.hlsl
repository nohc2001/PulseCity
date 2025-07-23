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

cbuffer CONSTANT_BUFFER_DEFAULT : register(b0)
{
    float4x4 model;
    float4x4 view;
    float4x4 project;
    PointLight pointLights;
    float3 CamPos;
}

struct VSInput
{
    float4 Pos : POSITION;
    float4 color : COLOR;
    float2 TexCoord : TEXCOORD0;
    float3 Normal : NORMAL0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
    float2 TexCoord : TEXCOORD0;
    float3 positionW : POSITION;
    float3 normalW : NORMAL0;
};

PSInput VSMain(VSInput input)
{
    PSInput result = (PSInput)0;
    result.color = input.color;
    result.TexCoord = input.TexCoord;
    result.positionW = (float3) mul(input.Pos, model);
    result.position = mul(mul(float4(result.positionW, 1.0f), view), project);
    result.normalW = mul(input.Normal, (float3x3)model);
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
  //  float attenuation = 4.0 * light.intencity / (2.0 / light.range + 5.0 * distance / light.range +
		//6.0 * light.intencity * (distance * distance) / light.range);
  //  attenuation = 2.0 * (Sigmoid(attenuation) - 0.5);

	// combine results
    //float3 ambient = light.ambient * material_ambient.Sample(samplerDiffuse, TexCoord).xyz;
    float3 diffuse = diff * material_diffuse.Sample(samplerDiffuse, TexCoord).xyz;
    //diffuse = material_diffuse.Sample(samplerDiffuse, TexCoord).xyz;
    float3 specular = spec * material_specular.Sample(samplerDiffuse, TexCoord).xyz;
    //ambient *= attenuation;
    //diffuse *= attenuation;
    //specular *= attenuation;
    return diffuse + specular; //(ambient + diffuse + specular);
}

float4 PSMain(PSInput input) : SV_TARGET
{
    input.normalW = normalize(input.normalW);
    
    float3 vCameraPosition = float3(CamPos.x, CamPos.y, CamPos.z);
    float3 vToCamera = normalize(vCameraPosition - input.positionW);
    float4 cColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
    
    //cColor += PointLight(0, input.positionW, input.normalW, vToCamera);
    float3 vToLight = pointLights.position - input.positionW;
    float fDistance = length(vToLight);

    float fSpecularFactor = 0.0f;
    vToLight /= fDistance;
    float fDiffuseFactor = dot(vToLight, input.normalW);
    float4 specular = material_specular.Sample(samplerDiffuse, input.TexCoord);
    if (fDiffuseFactor > 0.0f)
    {
        if (specular.a != 0.0f)
        {
            float3 vHalf = normalize(vToCamera + vToLight);
            fSpecularFactor = pow(max(dot(vHalf, input.normalW), 0.0f), specular.a);
        }
    }
    float4 diffuse = material_diffuse.Sample(samplerDiffuse, input.TexCoord);
    float fAttenuationFactor = 1.0f / dot( /*gLights[nIndex].m_vAttenuation*/float3(0.1f, 0.1f, 0.1f), float3(1.0f, fDistance, fDistance * fDistance));
    cColor += (((0.01f) + ( /*gLights[nIndex].m_cDiffuse*/float4(1, 1, 1, 1) * fDiffuseFactor * diffuse) + ( /*gLights[nIndex].m_cSpecular*/1.0f * fSpecularFactor * specular)) * fAttenuationFactor);
    
    float4 gcGlobalAmbientLight = float4(0.1f, 0.1f, 0.1f, 1.0f);
    cColor += (gcGlobalAmbientLight * /*gMaterial.m_cAmbient*/0.1f);
    cColor.a = diffuse.a;
    float4 color = cColor;
    return color;
    
    //float4 result = 0;
    //result += float4(input.mDiffuse, 1.0f);
    //result = result * material_diffuse.Sample(samplerDiffuse, input.TexCoord);
    
    //if (input.mDiffuse.x > 0.1f)
    //{
    //    float3 reflection = saturate(input.mReflection);
    //    float3 specular = saturate(1.5f*dot(reflection, -input.mViewDir));
    //    specular = pow(specular, 20.0f);
    //    result += float4(specular, 1.0f);
    //}
    
    //return result;
}
