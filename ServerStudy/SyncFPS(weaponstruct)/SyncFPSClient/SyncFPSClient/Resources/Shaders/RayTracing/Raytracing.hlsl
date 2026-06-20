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

#define ADD_PONG_SPECULAR
//#define DEBUG_NORMAL
#define MAX_TraceRayCount 6

// 청크 크기가 달라지면 바꾸어야 함.
#define CHUNCK_WIDTH 50

typedef float3 XMFLOAT3;
typedef float4 XMFLOAT4;
typedef float4 XMVECTOR;
typedef float4x4 XMMATRIX;
typedef uint UINT;

struct SceneConstantBuffer
{
    XMMATRIX projectionToWorld;
    XMVECTOR cameraPosition;
    float3 DirLight_invDirection;
    float DirLight_intencity;
    float3 DirLight_color;
    float extra;
    
    // 스테틱 라이팅을 위한 추가 요소.
    float4 ChunckStart; // 가장 인덱스가 작은 청크 시작점.
    uint4 ChunckCount; // 청크의 개수
};

//48byte (768 바이트 정렬)
struct Vertex
{
    float3 position; float u;
    float v; float3 normal;
    float3 tangent; uint extra;
};

//4byte (256 바이트 정렬)
struct TriangleIndex
{
    uint3 indexs;
};

//// Subobjects definitions at library scope. 
//GlobalRootSignature MyGlobalRootSignature =
//{
//    "DescriptorTable( UAV( u0 ) ),"                        // Output texture
//    "SRV( t0 ),"                                           // Acceleration structure
//    "CBV( b0 ),"                                           // Scene constants
//    "DescriptorTable( SRV( t1, numDescriptors = 2 ) )"     // Static index and vertex buffers.
//};

//LocalRootSignature MyLocalRootSignature =
//{
//    "RootConstants( num32BitConstants = 4, b1 )"           // Cube constants        
//};

//TriangleHitGroup MyHitGroup =
//{
//    "", // AnyHit
//    "MyClosestHitShader", // ClosestHit
//};

//SubobjectToExportsAssociation MyLocalRootSignatureAssociation =
//{
//    "MyLocalRootSignature", // subobject name
//    "MyHitGroup"             // export association 
//};

//RaytracingShaderConfig MyShaderConfig =
//{
//    16, // max payload size
//    8 // max attribute size
//};

//RaytracingPipelineConfig MyPipelineConfig =
//{
//    1 // max trace recursion depth
//};

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

struct Light
{
	// 라이트의 위치
    float3 pos;
    
    // 라이트의 개수
    int MaxLightCount;

	// 라이트의 방향
    float4 dir;

	// 라이트의 종류
    int lightType;

	// 스포트 라이트의 경우, 얼마나 넓게 퍼질것이냐를 결정함.
    float spot_angle;

	// 라이트가 반영되는 거리를 나타냄
    float range;

	// 라이트가 얼마나 강한지
    float intencity;

	// 빛의 색깔
    float4 LightColor;
};

struct ChunckLightData
{
    Light lights[32];
};

// global
RaytracingAccelerationStructure Scene : register(t0, space0);
RWTexture2D<float4> RenderTarget : register(u0);
RWTexture2D<float4> DepthStecil : register(u1);
StructuredBuffer<Vertex> Vertices : register(t1, space0);
StructuredBuffer<uint> Indices : register(t2, space0);
TextureCube SkyBoxCubeMap : register(t3, space0);
StructuredBuffer<Vertex> SkinMeshVertices : register(t4, space0);
SamplerState StaticSampler : register(s0);
StructuredBuffer<stMaterial> Materials : register(t5, space0);
//#define MAX_TEXTURES 1024
// Default Textures List
/*
0 : White (Defualt Diffuse)
1 : Blue (Default Normal)
*/
StructuredBuffer<ChunckLightData> ChunckStaticLightArr : register(t6); // t6 : Static Light Structured Buffer
Texture2D MaterialTexArr[MAX_TEXTURES] : register(t7); // t7 ~ t(7 + MAX_TEXUTURES)


ConstantBuffer<SceneConstantBuffer> g_sceneCB : register(b0);

//////////////////////////////////////////////////////////////////////
// 로컬
struct LocalRootSig
{
    uint VBOffset; // 버택스 인덱스 (바이트 오프셋 아님)
    uint IBOffset; // 인덱스 인덱스 (바이트 오프셋 아님)
    uint MaterialStart; // 머터리얼의 시작점.
};
ConstantBuffer<LocalRootSig> localCB : register(b1);

typedef BuiltInTriangleIntersectionAttributes MyAttributes;

struct RayPayload
{
    float4 color;
    float isCollide_Light;
    float depth;
    float stencil;
    float CalculationCount;
};

// Retrieve hit world position.
float3 HitWorldPosition()
{
    return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}

// Retrieve attribute at a hit position interpolated from vertex attributes using the hit's barycentrics.
float3 HitAttribute(float3 vertexAttribute[3], BuiltInTriangleIntersectionAttributes attr)
{
    return vertexAttribute[0] +
        attr.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
        attr.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

// Generate a ray in world space for a camera pixel corresponding to an index from the dispatched 2D grid.
inline void GenerateCameraRay(uint2 index, out float3 origin, out float3 direction)
{
    float2 xy = index + 0.5f; // center in the middle of the pixel.
    float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0 - 1.0;

    // Invert Y for DirectX-style coordinates.
    screenPos.y = -screenPos.y;

    // Unproject the pixel coordinate into a ray.
    float4 world = mul(float4(screenPos, 0, 1), g_sceneCB.projectionToWorld);

    world.xyz /= world.w;
    origin = g_sceneCB.cameraPosition.xyz;
    direction = normalize(world.xyz - origin);
}

int GetChunkIndexFromPosition(float3 pos)
{
    float3 cspos = pos - g_sceneCB.ChunckStart.xyz;
    cspos /= CHUNCK_WIDTH;
    cspos = floor(cspos);
    int index = int(cspos.z + cspos.y * g_sceneCB.ChunckCount.z + cspos.x * g_sceneCB.ChunckCount.z * g_sceneCB.ChunckCount.y);
    return index;
}

float CosAttenuation(float x)
{
    return 0.5 * cos(3.141592 * x) + 0.5;
}

// Diffuse lighting calculation.
float4 CalculateDiffuseLighting(float3 hitPosition, float3 normal)
{
    float3 pixelToLight = 1; //normalize(g_sceneCB.lightPosition.xyz - hitPosition);

    // Diffuse contribution.
    float fNDotL = max(0.0f, dot(pixelToLight, normal));

    return float4(1,1, 1, 1) * fNDotL;
}

// Base in LearnOpenGL PBR function modified
float Dfunction(float3 N, float3 H, float a, float3 V, float shininess)
{
    // 퐁 공식을 유지하면서 늘어짐만 추가하는 트릭
    float NdotH = max(dot(N, H), 0.0);
    // 시선이 낮아질수록(멀리 볼수록) 하이라이트를 강제로 옆으로 퍼뜨림
    float stretching = 1.0 / (max(dot(N, V), 0.0) + 0.1);
    float spec = pow(NdotH, shininess / stretching); // 거듭제곱 지수를 낮춰서 퍼뜨림
    
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

// 일단 함수를 만들어 놓긴 했는데 텍스쳐가 벽목도 아니며,
// 또한 밉맵을 한다고 자글거림이 효과적으로 제거되는것도 아닌것 같음.
int GetSampleLevel(float Distance, float3 Normal)
{
    const float far = 1000.0f;
    const float near = 0;
    const int maxMipLevel = 9;
    //float anglefactor = min(1.0f - dot(normalize(-ViewDir), Normal), 0);
    //float dist = min(pow(Distance / far, 1), 0);
    int SampleLevel = Distance / 10;
    return SampleLevel;
}

float CalculateMipFromDistance(float3 worldPos, float3 cameraPos, float3 normal, float fov)
{
    float dist = distance(worldPos, cameraPos);

    // 1. 거리에 따른 픽셀 크기 근사 (View Plane에서의 크기)
    // 거리가 2배 멀어지면 화면에 투영되는 텍셀 크기도 비례해서 커짐
    float pixelSizeAtDist = dist * tan(fov * 0.5) / (900 * 0.5);

    // 2. 표면 기울기(Normal)에 따른 보정
    // 시선 방향과 법선 사이의 각도가 클수록(비스듬할수록) 더 높은 밉맵 필요
    float3 viewDir = normalize(cameraPos - worldPos);
    float cosTheta = max(dot(normal, viewDir), 0.0001);

    // 3. 밉 레벨 공식: log2(거리 기반 스케일)
    // 실제 구현 시에는 텍스처 해상도와 정규화된 UV 단위를 고려한 상수가 필요함
    float lod = log2(pixelSizeAtDist * 1024 / cosTheta) - 4;

    return max(0.0, lod);
}

[shader("raygeneration")]
void MyRaygenShader()
{
    float3 rayDir;
    float3 origin;
    
    // Generate a ray for a camera pixel corresponding to an index from the dispatched 2D grid.
    GenerateCameraRay(DispatchRaysIndex().xy, origin, rayDir);

    // Trace the ray.
    // Set the ray's extents.
    RayDesc ray;
    ray.Origin = origin;
    
    //must normalize
    ray.Direction = rayDir;
    // Set TMin to a non-zero small value to avoid aliasing issues due to floating - point errors.
    // TMin should be kept small to prevent missing geometry at close contact areas.
    ray.TMin = 0.001;
    ray.TMax = 10000.0;
    
    RayPayload payload = { 0, 0, 0, 1, 0, 0, 0, 0 };
    TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);
    
    float3 color = payload.color.xyz;
    float exposure = 1; // 1.0보다 크면 더 밝음
    float3 postcolor = 1.0 - exp(-color * exposure); // exponential tone mapping
    postcolor = postcolor / (postcolor + float3(0.5f, 0.5f, 0.5f)); // HDR tonemapping
    postcolor = pow(postcolor, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2)); // gamma correct
    
    postcolor = payload.isCollide_Light * color + (1 - payload.isCollide_Light) * postcolor;
    
    // Write the raytraced color to the output texture.
    RenderTarget[DispatchRaysIndex().xy] = float4(postcolor, 1);
    DepthStecil[DispatchRaysIndex().xy].r = payload.depth;
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
    float3 hitPosition = HitWorldPosition();
    
    //Get3Vertexes
    Vertex vert[3];
    {
         // Get the base index of the triangle's first 32 bit index.
        const uint vertexSizeInByte = 44;
        const uint vertexPosOffset = 0;
        const uint vertexNormalOffset = 12;
        const uint vertexUvOffset = 24;
        const uint vertexTangentOffset = 32;
        
        const uint indexSizeInBytes = 4;
        const uint indicesPerTriangle = 3;
        const uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
        uint baseIndex = PrimitiveIndex();

        // Load up 3 16 bit indices for the triangle.
        uint3 indices;
        indices.x = Indices[localCB.IBOffset + 3 * baseIndex];
        indices.y = Indices[localCB.IBOffset + 3 * baseIndex + 1];
        indices.z = Indices[localCB.IBOffset + 3 * baseIndex + 2];
        
        //const uint3 indices = Load3x16BitIndices(baseIndex); // when 16 bit

        // Retrieve corresponding vertex normals for the triangle vertices.
        uint3 VertexStartOffset;
        VertexStartOffset.x = localCB.VBOffset + indices.x;
        VertexStartOffset.y = localCB.VBOffset + indices.y;
        VertexStartOffset.z = localCB.VBOffset + indices.z;
        
        vert[0] = Vertices[VertexStartOffset.x];
        vert[1] = Vertices[VertexStartOffset.y];
        vert[2] = Vertices[VertexStartOffset.z];
    }
    
    //Get UV
    float b1 = attr.barycentrics.x;
    float b2 = attr.barycentrics.y;
    float b0 = 1.0 - attr.barycentrics.x - attr.barycentrics.y;
    float2 hitUV = float2(0, 0);
    {
        hitUV = float2(vert[0].u, vert[0].v) * b0 + float2(vert[1].u, vert[1].v) * attr.barycentrics.x + float2(vert[2].u, vert[2].v) * attr.barycentrics.y;
    }
    
    //Get TBN (in World) 
    float3 Tangent;
    float3 Bitangent;
    float3 Normal;
    float3x3 WorldMat3x3 = WorldToObject3x4();
    {
        Normal = (b0 * vert[0].normal + b1 * vert[1].normal + b2 * vert[2].normal);
        Tangent = (b0 * vert[0].tangent + b1 * vert[1].tangent + b2 * vert[2].tangent);
        Normal = normalize(mul(Normal, WorldMat3x3));
        Tangent = normalize(mul(Tangent, WorldMat3x3));
        Bitangent = cross(Normal, Tangent);
    }
    
    //Get Sample Level
    
    float3 ViewDir = (hitPosition - g_sceneCB.cameraPosition.xyz);
    float Distance = length(ViewDir);
    ViewDir = normalize(ViewDir);
    float3 invViewDir = -ViewDir;
    float depth = min(Distance / 1000.0f, 1.0f);
    float SampleLevel = CalculateMipFromDistance(hitPosition, g_sceneCB.cameraPosition.xyz, Normal, 3.141592 / 3.0);
    float SampleInterploate = SampleLevel - floor(SampleLevel);
    SampleLevel = floor(SampleLevel);
    
     //Get Material & Tiling
    stMaterial material;
    material = Materials[max(localCB.MaterialStart, 0) + vert[0].extra];
    hitUV.x = material.TilingX * (hitUV.x + material.TilingOffsetX);
    hitUV.y = material.TilingY * (hitUV.y + material.TilingOffsetY);
    hitUV -= floor(hitUV);
    float3 Color = ((1.0 - SampleInterploate) * MaterialTexArr[material.diffuseTexId].SampleLevel(StaticSampler, hitUV, SampleLevel) 
        + SampleInterploate * MaterialTexArr[material.diffuseTexId].SampleLevel(StaticSampler, hitUV, SampleLevel + 1)) * material.baseColor;
    float3 TBNnormal = (1.0 - SampleInterploate) * MaterialTexArr[material.normalTexId].SampleLevel(StaticSampler, hitUV, SampleLevel) 
        + SampleInterploate * MaterialTexArr[material.normalTexId].SampleLevel(StaticSampler, hitUV, SampleLevel + 1);
    TBNnormal = TBNnormal.xyz;
    TBNnormal = 2.0 * (TBNnormal - 0.5);
    float AmbientOculusion = (1.0 - SampleInterploate) * MaterialTexArr[material.AOTexId].SampleLevel(StaticSampler, hitUV, SampleLevel).r
        + SampleInterploate * MaterialTexArr[material.AOTexId].SampleLevel(StaticSampler, hitUV, SampleLevel + 1).r;
    float Metalic = (1 - SampleInterploate) * MaterialTexArr[material.metalicTexId].SampleLevel(StaticSampler, hitUV, SampleLevel).r
        + SampleInterploate * MaterialTexArr[material.metalicTexId].SampleLevel(StaticSampler, hitUV, SampleLevel + 1).r;
    //float Roughness = min(0.5 + MaterialTexArr[material.roughnessTexId].SampleLevel(StaticSampler, hitUV, SampleLevel).r, 1.0);
    float Roughness = max(min(material.smoothness + MaterialTexArr[material.roughnessTexId].SampleLevel(StaticSampler, hitUV, SampleLevel).r, 1.0), 0.02f);
    
    //Real Normal (with NormalMap)
    float3x3 invTBN = 0;
    float3 realNormal = 0;
    {
        invTBN = transpose(float3x3(Tangent, Bitangent, Normal));
        realNormal = normalize(mul(invTBN, TBNnormal));
    }
    
#ifdef DEBUG_NORMAL
    payload.color = float4(realNormal, 1);
    payload.depth = depth;
    payload.stencil = 0.0f;
    payload.isCollide_Light = 0;
    return;
#endif
    
    //PBR Lighting Caculation Prepare
    float fValue = (1.0f - 0.02f) * Metalic + 0.02f;
    float3 Frenel0 = float3(fValue, fValue, fValue);
    Frenel0 = lerp(Frenel0, Color, Metalic);
    float3 albedo = pow(Color, float3(2.2, 2.2, 2.2));
    const float CalculationCount = 1;
    float sum = 0;
    float dW = 1.0 / CalculationCount;
    float3 Lo = float3(0.0, 0, 0);
    float3 diffuseColor = 0;
    
    //Lighting Calculation
    float ShadowRate = 1.0f;
    float RayCount = 0;
    {
        //direction Light Calculation
        // shadowRay
        ShadowRate = 0.0f;
        RayDesc ray;
        ray.Origin = hitPosition;
        float3 vec = g_sceneCB.DirLight_invDirection; /*g_sceneCB.lightPosition.xyz - hitPosition; //DirLightDir;*/
        ray.Direction = normalize(vec);
        ray.TMin = 0.001;
        ray.TMax = 10000.0f; /*length(vec);*/
        RayPayload LightPayload = { 0, 0, 0, 1, 0, 0, 0, 0 };
        LightPayload.CalculationCount = payload.CalculationCount;
        TraceRay(Scene, RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, ~0, 0, 1, 0, ray, LightPayload);
        ShadowRate = LightPayload.isCollide_Light;
        float3 wi = normalize(g_sceneCB.DirLight_invDirection);
        float3 H = normalize(invViewDir + wi);
        float3 F = Ffunction(max(dot(H, wi), 0), Frenel0);
        float pbrRough = max(Roughness, 0.02f);
        float3 Result = PBRonOneLightRay(invViewDir, wi, realNormal, Frenel0, pbrRough, Color.xyz, g_sceneCB.DirLight_color, hitPosition);
        // 이건 현실적인 렌더링은 아니다.
        // 하지만 퐁 스페큘러를 추가하는 편이 더 보기가 좋더라
#ifdef ADD_PONG_SPECULAR
        float3 reflectDir = reflect(normalize(-g_sceneCB.DirLight_invDirection), realNormal);
        float specular = pow(saturate(dot(invViewDir, reflectDir)), 10.0f - 10 * pbrRough); // dot half vector & normal
        float3 specularColor = g_sceneCB.DirLight_color.xyz * specular * 0.25f;
        Result = saturate(Result + specularColor * ShadowRate);
#endif
        Lo += ShadowRate * Result;
        payload.CalculationCount += 1;
        RayCount += 1;
        
        //Chunck Static Light
        ChunckLightData cld = ChunckStaticLightArr[GetChunkIndexFromPosition(hitPosition)];
        int AdditionLightCount = cld.lights[0].MaxLightCount;
        for (int i = 0; i < AdditionLightCount; ++i)
        {
            Light l = cld.lights[i];
            float3 LightVector = hitPosition - l.pos;
            float Lightlen = length(LightVector);
            float rate = max(1 - (Lightlen / l.range), 0);
            LightVector = normalize(LightVector);
            bool b = (l.lightType == 0 && (dot(LightVector, l.dir.xyz) > cos(0.5 * 3.141592 * l.spot_angle / 180) && rate > 0));
            b = b || (l.lightType == 2 && rate > 0);

            if (b) // SpotLight
            {
                payload.CalculationCount += 1;
                ShadowRate = 0.0f;
                RayDesc ray;
                ray.Origin = hitPosition;
                ray.Direction = -LightVector;
                ray.TMin = 0.001;
                ray.TMax = Lightlen;
                RayPayload ShadowLightPayload = { 0, 0, 0, 1, 0, 0, 0, 0 };
                TraceRay(Scene, RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, ~0, 0, 1, 0, ray, ShadowLightPayload);
                ShadowRate = ShadowLightPayload.isCollide_Light;
                wi = -LightVector;
                float3 Radiance = l.LightColor * l.intencity * CosAttenuation(1 - rate);
                Lo += ShadowRate * PBRonOneLightRay(invViewDir, wi, realNormal, Frenel0, Roughness, Color.xyz, Radiance, hitPosition);
                RayCount += 1;
            }
        }
        
        float Reflectrate = min(length(Lo), 1);
        // Reflection
        if (MAX_TraceRayCount > payload.CalculationCount)
        {
            wi = reflect(ViewDir, realNormal);
            ray.Origin = hitPosition;
            ray.Direction = wi;
            ray.TMin = 0.001;
            ray.TMax = 2000.0f * pbrRough; /*length(vec);*/
            RayPayload rpayload = { 0, 0, 0, 1, 0, 0, 0, 0 };
            rpayload.CalculationCount = payload.CalculationCount;
            TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, rpayload);
            float3 reflectColor = rpayload.color.xyz;
            Lo += Reflectrate * PBRonOneLightRay_IBL(invViewDir, wi, realNormal, Frenel0, pbrRough, Color.xyz, reflectColor, hitPosition);
            payload.CalculationCount = rpayload.CalculationCount;
            RayCount += 1;
        }
    }
    //Lo /= RayCount;
    
    float3 ambient = float3(0.03, 0.03, 0.03) * albedo * AmbientOculusion;
    payload.color = float4(Lo + ambient, 1);
    payload.depth = depth;
    payload.stencil = 0.0f;
    payload.isCollide_Light = 0;
}

[shader("closesthit")]
void MySkinMeshClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
    float3 hitPosition = HitWorldPosition();
    
    //Get3Vertexes
    Vertex vert[3];
    {
         // Get the base index of the triangle's first 32 bit index.
        const uint vertexSizeInByte = 44;
        const uint vertexPosOffset = 0;
        const uint vertexNormalOffset = 12;
        const uint vertexUvOffset = 24;
        const uint vertexTangentOffset = 32;
        
        const uint indexSizeInBytes = 4;
        const uint indicesPerTriangle = 3;
        const uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
        uint baseIndex = PrimitiveIndex();

        // Load up 3 16 bit indices for the triangle.
        uint3 indices;
        indices.x = Indices[localCB.IBOffset + 3 * baseIndex];
        indices.y = Indices[localCB.IBOffset + 3 * baseIndex + 1];
        indices.z = Indices[localCB.IBOffset + 3 * baseIndex + 2];
        
        //const uint3 indices = Load3x16BitIndices(baseIndex); // when 16 bit

        // Retrieve corresponding vertex normals for the triangle vertices.
        uint3 VertexStartOffset;
        VertexStartOffset.x = localCB.VBOffset + indices.x;
        VertexStartOffset.y = localCB.VBOffset + indices.y;
        VertexStartOffset.z = localCB.VBOffset + indices.z;
        
        vert[0] = SkinMeshVertices[VertexStartOffset.x];
        vert[1] = SkinMeshVertices[VertexStartOffset.y];
        vert[2] = SkinMeshVertices[VertexStartOffset.z];
    }
    
    //Get UV
    float b1 = attr.barycentrics.x;
    float b2 = attr.barycentrics.y;
    float b0 = 1.0 - attr.barycentrics.x - attr.barycentrics.y;
    float2 hitUV = float2(0, 0);
    {
        hitUV = float2(vert[0].u, vert[0].v) * b0 + float2(vert[1].u, vert[1].v) * attr.barycentrics.x + float2(vert[2].u, vert[2].v) * attr.barycentrics.y;
    }
    
    //Get TBN (in World) 
    float3 Tangent;
    float3 Bitangent;
    float3 Normal;
    float3x3 WorldMat3x3 = WorldToObject3x4();
    {
        Normal = (b0 * vert[0].normal + b1 * vert[1].normal + b2 * vert[2].normal);
        Tangent = (b0 * vert[0].tangent + b1 * vert[1].tangent + b2 * vert[2].tangent);
        Normal = normalize(mul(Normal, WorldMat3x3));
        Tangent = normalize(mul(Tangent, WorldMat3x3));
        Bitangent = cross(Normal, Tangent);
    }
    
    //Get Sample Level
    float3 ViewDir = normalize(hitPosition - g_sceneCB.cameraPosition.xyz);
    float3 invViewDir = -ViewDir;
    float Distance = length(ViewDir);
    float depth = min(Distance / 1000.0f, 1.0f);
    float SampleLevel = CalculateMipFromDistance(hitPosition, g_sceneCB.cameraPosition.xyz, Normal, 3.141592 / 3.0);
    
     //Get Material & Tiling
    stMaterial material;
    material = Materials[max(localCB.MaterialStart, 0) + vert[0].extra];
    hitUV.x = material.TilingX * (hitUV.x + material.TilingOffsetX);
    hitUV.y = material.TilingY * (hitUV.y + material.TilingOffsetY);
    hitUV -= floor(hitUV);
    float3 Color = MaterialTexArr[material.diffuseTexId].SampleLevel(StaticSampler, hitUV, SampleLevel) * material.baseColor;
    float3 TBNnormal = MaterialTexArr[material.normalTexId].SampleLevel(StaticSampler, hitUV, SampleLevel);
    TBNnormal = TBNnormal.xyz;
    TBNnormal = 2.0 * (TBNnormal - 0.5);
    float AmbientOculusion = MaterialTexArr[material.AOTexId].SampleLevel(StaticSampler, hitUV, SampleLevel).r;
    float Metalic = MaterialTexArr[material.metalicTexId].SampleLevel(StaticSampler, hitUV, SampleLevel).r;
    //float Roughness = min(0.5 + MaterialTexArr[material.roughnessTexId].SampleLevel(StaticSampler, hitUV, SampleLevel).r, 1.0);
    float Roughness = max(min(material.smoothness + MaterialTexArr[material.roughnessTexId].SampleLevel(StaticSampler, hitUV, SampleLevel).r, 1.0), 0.02f);
    
    //Real Normal (with NormalMap)
    float3x3 invTBN = 0;
    float3 realNormal = 0;
    {
        invTBN = /*transpose*/(float3x3(Tangent, Bitangent, Normal));
        realNormal = normalize(mul(invTBN, TBNnormal));
    }
    
    //PBR Lighting Caculation Prepare
    float3 Frenel0 = float3(0.02, 0.02, 0.02);
    Frenel0 = lerp(Frenel0, Color, Metalic);
    float3 albedo = pow(Color, float3(2.2, 2.2, 2.2));
    const float CalculationCount = 1;
    float sum = 0;
    float dW = 1.0 / CalculationCount;
    float3 Lo = float3(0.0, 0, 0);
    float3 diffuseColor = 0;
    
      //Lighting Calculation
    float ShadowRate = 1.0f;
    float RayCount = 0;
    {
       //direction Light Calculation
        // shadowRay
        ShadowRate = 0.0f;
        RayDesc ray;
        ray.Origin = hitPosition;
        float3 vec = g_sceneCB.DirLight_invDirection; /*g_sceneCB.lightPosition.xyz - hitPosition; //DirLightDir;*/
        ray.Direction = normalize(vec);
        ray.TMin = 0.001;
        ray.TMax = 10000.0f; /*length(vec);*/
        RayPayload LightPayload = { 0, 0, 0, 1, 0, 0, 0, 0 };
        LightPayload.CalculationCount = payload.CalculationCount;
        TraceRay(Scene, RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, ~0, 0, 1, 0, ray, LightPayload);
        ShadowRate = LightPayload.isCollide_Light;
        float3 wi = normalize(g_sceneCB.DirLight_invDirection);
        float3 H = normalize(invViewDir + wi);
        float3 F = Ffunction(max(dot(H, wi), 0), Frenel0);
        float pbrRough = max(Roughness, 0.02f);
        float3 Result = PBRonOneLightRay(invViewDir, wi, realNormal, Frenel0, pbrRough, Color.xyz, g_sceneCB.DirLight_color, hitPosition);
        // 이건 현실적인 렌더링은 아니다.
        // 하지만 퐁 스페큘러를 추가하는 편이 더 보기가 좋더라
#ifdef ADD_PONG_SPECULAR
        float3 reflectDir = reflect(normalize(-g_sceneCB.DirLight_invDirection), realNormal);
        float specular = pow(saturate(dot(invViewDir, reflectDir)), 10.0f - 10 * pbrRough); // dot half vector & normal
        float3 specularColor = g_sceneCB.DirLight_color.xyz * specular * 0.25f;
        Result = saturate(Result + specularColor * ShadowRate);
#endif
        Lo += ShadowRate * Result;
        payload.CalculationCount += 1;
        RayCount += 1;
        
        //Chunck Static Light
        ChunckLightData cld = ChunckStaticLightArr[GetChunkIndexFromPosition(hitPosition)];
        int AdditionLightCount = cld.lights[0].MaxLightCount;
        for (int i = 0; i < AdditionLightCount; ++i)
        {
            Light l = cld.lights[i];
            float3 LightVector = hitPosition - l.pos;
            float Lightlen = length(LightVector);
            float rate = max(1 - (Lightlen / l.range), 0);
            LightVector = normalize(LightVector);
            bool b = (l.lightType == 0 && (dot(LightVector, l.dir.xyz) > cos(0.5 * 3.141592 * l.spot_angle / 180) && rate > 0));
            b = b || (l.lightType == 2 && rate > 0);

            if (b) // SpotLight
            {
                payload.CalculationCount += 1;
                ShadowRate = 0.0f;
                RayDesc ray;
                ray.Origin = hitPosition;
                ray.Direction = -LightVector;
                ray.TMin = 0.001;
                ray.TMax = Lightlen;
                RayPayload ShadowLightPayload = { 0, 0, 0, 1, 0, 0, 0, 0 };
                TraceRay(Scene, RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, ~0, 0, 1, 0, ray, ShadowLightPayload);
                ShadowRate = ShadowLightPayload.isCollide_Light;
                wi = -LightVector;
                float3 Radiance = l.LightColor * l.intencity * CosAttenuation(1 - rate);
                Lo += ShadowRate * PBRonOneLightRay(invViewDir, wi, realNormal, Frenel0, Roughness, Color.xyz, Radiance, hitPosition);
                RayCount += 1;
            }
        }
        
        float Reflectrate = min(length(Lo), 1);
        // Reflection
        if (MAX_TraceRayCount > payload.CalculationCount)
        {
            wi = reflect(ViewDir, realNormal);
            ray.Origin = hitPosition;
            ray.Direction = wi;
            ray.TMin = 0.001;
            ray.TMax = 2000.0f * pbrRough; /*length(vec);*/
            RayPayload rpayload = { 0, 0, 0, 1, 0, 0, 0, 0 };
            rpayload.CalculationCount = payload.CalculationCount;
            TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, rpayload);
            float3 reflectColor = rpayload.color.xyz;
            Lo += Reflectrate * PBRonOneLightRay_IBL(invViewDir, wi, realNormal, Frenel0, pbrRough, Color.xyz, reflectColor, hitPosition);
            payload.CalculationCount = rpayload.CalculationCount;
            RayCount += 1;
        }
    }
    Lo /= RayCount;
    
    float3 ambient = float3(0.03, 0.03, 0.03) * albedo * AmbientOculusion;
    payload.color = float4(Lo + ambient, 1);
    payload.depth = depth;
    payload.stencil = 0.0f;
    payload.isCollide_Light = 0;
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
    float3 raydir = normalize(WorldRayDirection());
    payload.color = SkyBoxCubeMap.SampleLevel(StaticSampler, raydir, 0);
    payload.color.w = 1;
    payload.isCollide_Light = 1.0f;
    payload.depth = 1.0f;
    payload.stencil = 0.0f;
}