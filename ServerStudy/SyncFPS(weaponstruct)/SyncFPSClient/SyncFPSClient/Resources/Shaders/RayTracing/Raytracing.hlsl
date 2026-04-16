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
Texture2D MaterialTexArr[MAX_TEXTURES] : register(t6); // t6 ~ t(6 + MAX_TEXUTURES)

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
    float3 ReflectRayStart;
    float isCollide_Light;
    float3 ReflectRaydir;
    float RelectRate;
    float depth;
    float stencil;
    float2 extra;
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

// Diffuse lighting calculation.
float4 CalculateDiffuseLighting(float3 hitPosition, float3 normal)
{
    float3 pixelToLight = 1; //normalize(g_sceneCB.lightPosition.xyz - hitPosition);

    // Diffuse contribution.
    float fNDotL = max(0.0f, dot(pixelToLight, normal));

    return float4(1,1, 1, 1) * fNDotL;
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
    
    RayPayload payload = { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0 , 0, 0, 0, 0};
    TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);
    float reflectRate = payload.RelectRate;

    ray.Origin = payload.ReflectRayStart;
    ray.Direction = payload.ReflectRaydir;
    RayPayload rpayload = { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0 , 0, 0, 0,0};
    TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, rpayload);
    float4 reflectColor = rpayload.color;
    
    // Write the raytraced color to the output texture.
    RenderTarget[DispatchRaysIndex().xy] = (1 - reflectRate) * payload.color + (reflectRate) * reflectColor;
    DepthStecil[DispatchRaysIndex().xy].r = payload.depth;
}

//PBR Functions
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
    float G = Gfunction(N, ViewDir, Wi, K_IBL(Roughness));
    float denom = 4 * max(dot(ViewDir, N), 0.0) * max(dot(Wi, N), 0.0) + 1e-5;
    return D * Frenel * G / denom;
}

float3 BRDF_lambert(float3 color)
{
    return color / 3.141592f;
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
    float3 ViewDir = hitPosition - g_sceneCB.cameraPosition.xyz;
    float Distance = length(ViewDir);
    float depth = min(Distance / 1000.0f, 1.0f);
    int SampleLevel = 0;//GetSampleLevel(Distance, Normal);
    
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
    float Roughness = min(0.5 + MaterialTexArr[material.roughnessTexId].SampleLevel(StaticSampler, hitUV, SampleLevel).r, 1.0);
    
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
    //direction Light Calculation
    {
        // shadowRay
        float ShadowRate = 1.0f;
        ShadowRate = 0.0f;
        RayDesc ray;
        ray.Origin = hitPosition;
        float3 vec = g_sceneCB.DirLight_invDirection; /*g_sceneCB.lightPosition.xyz - hitPosition; //DirLightDir;*/
        ray.Direction = normalize(vec);
        ray.TMin = 0.001;
        ray.TMax = 10000.0f; /*length(vec);*/
        payload.isCollide_Light = 0;
        TraceRay(Scene, RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, ~0, 0, 1, 0, ray, payload);
        ShadowRate = payload.isCollide_Light;
        
        float3 wi = normalize(g_sceneCB.DirLight_invDirection);
        //float3 reflectV = reflect(wi, realNormal);
        float3 H = normalize(ViewDir + wi);
        float3 radiance = g_sceneCB.DirLight_color;
        // Cook-Torrance BRDF
        float3 F = Ffunction(max(dot(H, wi), 0), Frenel0);
        // scale light by NdotL
        float NdotL = max(dot(realNormal, wi), 0.0);
        // add to outgoing radiance Lo
        Lo += ShadowRate * dW * (BRDF_lambert(Color.xyz) + BRDF_cookToorrance(hitPosition, wi, ViewDir, realNormal, Roughness, F)) * radiance * NdotL;
    }
    
    // PBR Post Process
    float3 ambient = float3(0.03, 0.03, 0.03) * albedo * AmbientOculusion;
    float3 color = ambient + Lo;
    float exposure = 1; // 1.0보다 크면 더 밝음
    color = 1.0 - exp(-color * exposure); // exponential tone mapping
    color = color / (color + float3(0.5f, 0.5f, 0.5f)); // HDR tonemapping
    color = pow(color, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2)); // gamma correct
    
    //reflect
    payload.ReflectRayStart = hitPosition;
    float3 raydir = WorldRayDirection();
    float3 reflectNormal = ((1.0f - depth) * realNormal + depth * Normal);
    payload.ReflectRaydir = normalize(reflect(raydir, reflectNormal));
    payload.RelectRate = 0.5f * material.smoothness * (1.0f - depth);
    payload.color = /*float4(SampleLevel * 0.1f, SampleLevel * 0.1f, SampleLevel * 0.1f, 1.0f);*/ float4(color, 1);
    payload.depth = depth;
    payload.stencil = 0.0f;
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
    float3 ViewDir = hitPosition - g_sceneCB.cameraPosition.xyz;
    float Distance = length(ViewDir);
    float depth = max(Distance / 1000.0f, 1.0f);
    int SampleLevel = 0; //GetSampleLevel(Distance, Normal);
    
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
    float Roughness = min(0.5 + MaterialTexArr[material.roughnessTexId].SampleLevel(StaticSampler, hitUV, SampleLevel).r, 1.0);
    
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
    //direction Light Calculation
    {
        // shadowRay
        float ShadowRate = 1.0f;
        ShadowRate = 0.0f;
        RayDesc ray;
        ray.Origin = hitPosition;
        float3 vec = g_sceneCB.DirLight_invDirection; /*g_sceneCB.lightPosition.xyz - hitPosition; //DirLightDir;*/
        ray.Direction = normalize(vec);
        ray.TMin = 0.001;
        ray.TMax = 10000.0f; /*length(vec);*/
        payload.isCollide_Light = 0;
        TraceRay(Scene, RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, ~0, 0, 1, 0, ray, payload);
        ShadowRate = payload.isCollide_Light;
        
        float3 wi = normalize(g_sceneCB.DirLight_invDirection);
        float3 reflectV = reflect(wi, realNormal);
        float3 H = normalize(ViewDir + wi);
        float3 radiance = g_sceneCB.DirLight_color;
        // Cook-Torrance BRDF
        float3 F = Ffunction(max(dot(H, wi), 0), Frenel0);
        // scale light by NdotL
        float NdotL = max(dot(realNormal, wi), 0.0);
        // add to outgoing radiance Lo
        Lo += ShadowRate * dW * (BRDF_lambert(Color.xyz) + BRDF_cookToorrance(hitPosition, wi, ViewDir, realNormal, Roughness, F)) * radiance * NdotL;
    }
    // PBR Post Process
    float3 ambient = float3(0.03, 0.03, 0.03) * albedo * AmbientOculusion;
    float3 color = ambient + Lo;
    float exposure = 1; // 1.0보다 크면 더 밝음
    color = 1.0 - exp(-color * exposure); // exponential tone mapping
    color = color / (color + float3(0.5f, 0.5f, 0.5f)); // HDR tonemapping
    color = pow(color, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2)); // gamma correct
    
    //reflect
    payload.ReflectRayStart = hitPosition;
    float3 raydir = WorldRayDirection();
    payload.ReflectRaydir = normalize(reflect(raydir, realNormal));
    payload.RelectRate = 0.5f * material.smoothness * (1-depth);
    payload.color = float4(color, 1);
    payload.depth = depth;
    payload.stencil = 0.0f;
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
    float3 raydir = normalize(WorldRayDirection());
    payload.color = SkyBoxCubeMap.SampleLevel(StaticSampler, raydir, 0);
    payload.color.w = 1;
    payload.ReflectRaydir = raydir;
    payload.isCollide_Light = 1.0f;
    payload.depth = 1.0f;
    payload.stencil = 0.0f;
}