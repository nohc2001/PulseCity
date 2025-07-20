cbuffer cbCameraInfo : register(b0)
{
    matrix gProjection;
    matrix gView;
};

cbuffer cbWorldInfo : register(b1)
{
    matrix gWorld;
};

cbuffer cbLightInfo : register(b2)
{
    float3 gLightDirection; // Light Direction
    float4 gLightColor; // Light_Color
    float4 gAmbientColor; // Ambient_Color
    float3 gEyePosition; // camera location
}
struct VS_INPUT
{
    float3 position : POSITION;
    float4 color : COLOR;
    float3 normal : NORMAL;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float3 normalW : NORMAL;
    float4 positionW : POSITION;
};

VS_OUTPUT VSMain(VS_INPUT input)
{
    VS_OUTPUT output;
    output.positionW = mul(float4(input.position, 1.0f), gWorld);
    output.position = mul(mul(output.positionW, gView), gProjection);
    output.color = input.color;
    output.normalW = mul(input.normal, (float3x3) gWorld);
    return (output);
}
 //«»ºø ºŒ¿Ã¥ı∏¶ ¡§¿««—¥Ÿ.
float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
    // Temporary Light Value
    float3 tempLightDirection = normalize(float3(0.0f, -1.0f, 0.0f)); // Light Direction
    float4 tempLightColor = float4(1.0f, 1.0f, 1.0f, 1.0f); // Light_Color
    float4 tempAmbientColor = float4(0.2f, 0.2f, 0.2f, 1.0f); // Ambient_Color
    
    // get from world space : normal, location vector
    float3 normalW = normalize(input.normalW);
    float3 positionW = input.positionW.xyz;
    
    // Ambient
    float3 ambient = tempAmbientColor.rgb;
    
    // Diffuse
    float3 lightVector = normalize(-tempLightDirection);
    float diffuse = saturate(dot(normalW, lightVector)); // dot normal & light direction 
    float3 diffuseColor = tempLightColor.rgb * diffuse;
    
    // Specular
    float3 eyeVector = normalize(gEyePosition - positionW);
    float3 halfVector = normalize(lightVector + eyeVector);
    float specular = pow(saturate(dot(normalW, halfVector)), 16.0f); // dot half vector & normal
    float3 specularColor = tempLightColor.rgb * specular;
    
    // Result Color
    float3 finalColor = ambient + (diffuseColor * input.color.rgb) + (specularColor * input.color.rgb);
   
    return float4(finalColor, input.color.a);
}