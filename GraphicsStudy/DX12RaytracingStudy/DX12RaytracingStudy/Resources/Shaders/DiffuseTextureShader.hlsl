cbuffer cbCameraInfo : register(b0)
{
    matrix gProjection;
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

cbuffer cbLightInfo : register(b2)
{
    DirectionLight dirLight;
    PointLight pointLightArr[8];
}

Texture2D texDiffuse : register(t0);
SamplerState StaticSampler : register(s0);

struct VS_INPUT
{
    float3 position : POSITION;
    float4 color : COLOR;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float3 normalW : NORMAL;
    float4 positionW : POSITION;
    float3 cameraPosW : TEXCOORD0;
    float2 uv : TEXCOORD1;
};

float Sigmoid(float x)
{
    return 1.0 / (1.0 + (exp(-x)));
}

VS_OUTPUT VSMain(VS_INPUT input)
{
    VS_OUTPUT output;
    output.positionW = mul(float4(input.position, 1.0f), gWorld);
    output.position = mul(mul(output.positionW, gView), gProjection);
    output.color = input.color;
    output.normalW = mul(input.normal, (float3x3) gWorld);
    output.cameraPosW = mul(Camera_Position, gWorld);
    output.uv = input.uv;
    return (output);
}

struct GS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float3 normalW : NORMAL;
    float4 positionW : POSITION;
    float3 cameraPosW : TEXCOORD0;
    float2 uv : TEXCOORD1;
};

 //«»ºø ºŒ¿Ã¥ı∏¶ ¡§¿««—¥Ÿ.
float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
    const float3 LightPos = float3(0.0f, 1.0f, 0.0f);
    const float LightIntencity = 100.0f;
    const float LightRange = 30.0f;
    const float4 tempLightColor = float4(1.0f, 1.0f, 1.0f, 1.0f); // Light_Color
    const float4 tempAmbientColor = float4(0.01f, 0.01f, 0.01f, 1.0f); // Ambient_Color
    
    float3 diffuseTexColor = texDiffuse.Sample(StaticSampler, input.uv);
    //return float4(diffuseTexColor, input.color.a);
    
    // get from world space : normal, location vector
    float3 normalW = normalize(input.normalW);
    float3 positionW = input.positionW.xyz;
    
    //attenuation
    float distance = length(LightPos - positionW);
    float attenuation = 4.0 * LightIntencity / (2.0 / LightRange + 5.0 * distance / LightRange +
		6.0 * LightIntencity * (distance * distance) / LightRange);
    attenuation = 2.0 * (Sigmoid(attenuation) - 0.5);
    
    // Ambient
    float3 ambient = tempAmbientColor.rgb;
    
    // Diffuse
    float3 LightDirection = normalize(positionW - LightPos);
    float3 lightVector = -normalize(LightDirection - LightPos);
    float diffuse = saturate(dot(normalW, lightVector)); // dot normal & light direction 
    float3 diffuseColor = tempLightColor.rgb * diffuseTexColor * diffuse;
    
    // Specular
    float3 reflectDir = reflect(-LightDirection, normalW);
    float3 viewDir = normalize(positionW - input.cameraPosW);
    //float3 halfVector = normalize(lightVector + eyeVector);
    float specular = pow(saturate(dot(viewDir, reflectDir)), 32.0f); // dot half vector & normal
    float3 specularColor = 0.25f * tempLightColor.rgb * specular;
    
    // Result Color
    float3 finalColor = attenuation * ambient + attenuation * (diffuseColor * input.color.rgb) + attenuation * (specularColor * input.color.rgb);
   
    return float4(finalColor, input.color.a);
}