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
    float3 normal; float v;
    float3 tangent; float extra;
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


RaytracingAccelerationStructure Scene : register(t0, space0);

RWTexture2D<float4> RenderTarget : register(u0);

StructuredBuffer<Vertex> Vertices : register(t1, space0);
StructuredBuffer<uint> Indices : register(t2, space0);
TextureCube SkyBoxCubeMap : register(t3, space0);
SamplerState StaticSampler : register(s0);

ConstantBuffer<SceneConstantBuffer> g_sceneCB : register(b0);

struct LocalRootSig
{
    uint VBOffset; // 버택스 인덱스 (바이트 오프셋 아님)
    uint IBOffset; // 인덱스 인덱스 (바이트 오프셋 아님)
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
    
    RayPayload payload = { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
    TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);
    float reflectRate = payload.RelectRate;

    ray.Origin = payload.ReflectRayStart;
    ray.Direction = payload.ReflectRaydir;
    RayPayload rpayload = { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
    TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, rpayload);
    float4 reflectColor = rpayload.color;
    
    // Write the raytraced color to the output texture.
    RenderTarget[DispatchRaysIndex().xy] = (1 - reflectRate) * payload.color + (reflectRate) * reflectColor;
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
    float3 hitPosition = HitWorldPosition();
   
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
   
    //GetNormal
    float3 triangleNormal;
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
        indices.x = Indices[localCB.IBOffset + 3*baseIndex];
        indices.y = Indices[localCB.IBOffset + 3*baseIndex + 1];
        indices.z = Indices[localCB.IBOffset + 3*baseIndex + 2];
        
        //const uint3 indices = Load3x16BitIndices(baseIndex); // when 16 bit

        // Retrieve corresponding vertex normals for the triangle vertices.
        uint3 VertexStartOffset;
        VertexStartOffset.x = localCB.VBOffset + indices.x;
        VertexStartOffset.y = localCB.VBOffset + indices.y;
        VertexStartOffset.z = localCB.VBOffset + indices.z;
        
        float3 vertexNormals[3] =
        {
            (Vertices[VertexStartOffset.x].normal),
            (Vertices[VertexStartOffset.y].normal),
            (Vertices[VertexStartOffset.z].normal),
        };
        
        // Compute the triangle's normal.
        // This is redundant and done for illustration purposes 
        // as all the per-vertex normals are the same and match triangle's normal in this sample. 
        //float3 triangleNormal = HitAttribute(vertexNormals, attr);
        triangleNormal = (vertexNormals[0] + vertexNormals[1] + vertexNormals[2]) / 3;
        float3x3 mat3x3 = WorldToObject3x4();
        triangleNormal = mul(triangleNormal, mat3x3);
        
        // fix : 이걸 이렇게 정규화하는 이유? 왜인지 모르겠는데 일부 삼각형들의 normal 이 아주 작아져서 그럼.
        triangleNormal = (normalize(triangleNormal));
    }
    
    payload.ReflectRayStart = hitPosition;
    float3 raydir = WorldRayDirection();
    payload.ReflectRaydir = normalize(reflect(raydir, triangleNormal));
    payload.RelectRate = 0.5f;
    
    float fNDotL = max(0.0f, dot(g_sceneCB.DirLight_invDirection, triangleNormal));
    float4 diffuseColor = g_sceneCB.DirLight_intencity * float4(g_sceneCB.DirLight_color, 1) * fNDotL;
    float4 color = diffuseColor;
    color.xyz *= ShadowRate;
    color.w = 1;
    payload.color = color;    
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
    float3 raydir = normalize(WorldRayDirection());
    payload.color = SkyBoxCubeMap.SampleLevel(StaticSampler, raydir, 0);
    payload.color.w = 1;
    payload.ReflectRaydir = raydir;
    payload.isCollide_Light = 1.0f;
}