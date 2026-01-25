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
    XMVECTOR lightPosition;
};

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT3 normal;
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
ByteAddressBuffer Indices : register(t2, space0);

ConstantBuffer<SceneConstantBuffer> g_sceneCB : register(b0);

struct LocalRootSig
{
    uint VBOffset;
    uint IBOffset;
};
ConstantBuffer<LocalRootSig> localCB : register(b1);

// Load three 16 bit indices from a byte addressed buffer.
uint3 Load3x16BitIndices(uint offsetBytes)
{
    uint3 indices;

    // ByteAdressBuffer loads must be aligned at a 4 byte boundary.
    // Since we need to read three 16 bit indices: { 0, 1, 2 } 
    // aligned at a 4 byte boundary as: { 0 1 } { 2 0 } { 1 2 } { 0 1 } ...
    // we will load 8 bytes (~ 4 indices { a b | c d }) to handle two possible index triplet layouts,
    // based on first index's offsetBytes being aligned at the 4 byte boundary or not:
    //  Aligned:     { 0 1 | 2 - }
    //  Not aligned: { - 0 | 1 2 }
    const uint dwordAlignedOffset = offsetBytes & ~3;
    const uint2 four16BitIndices = Indices.Load2(dwordAlignedOffset);
 
    // Aligned: { 0 1 | 2 - } => retrieve first three 16bit indices
    if (dwordAlignedOffset == offsetBytes)
    {
        indices.x = four16BitIndices.x & 0xffff;
        indices.y = (four16BitIndices.x >> 16) & 0xffff;
        indices.z = four16BitIndices.y & 0xffff;
    }
    else // Not aligned: { - 0 | 1 2 } => retrieve last three 16bit indices
    {
        indices.x = (four16BitIndices.x >> 16) & 0xffff;
        indices.y = four16BitIndices.y & 0xffff;
        indices.z = (four16BitIndices.y >> 16) & 0xffff;
    }

    return indices;
}

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
    float3 pixelToLight = normalize(g_sceneCB.lightPosition.xyz - hitPosition);

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
    
    RayPayload payload = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);
    float reflectRate = payload.RelectRate;

    ray.Origin = payload.ReflectRayStart;
    ray.Direction = payload.ReflectRaydir;
    RayPayload rpayload = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, rpayload);
    float4 reflectColor = rpayload.color;
    
    // Write the raytraced color to the output texture.
    RenderTarget[DispatchRaysIndex().xy] = (1 - reflectRate) * payload.color + (reflectRate) * reflectColor;
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
    float3 hitPosition = HitWorldPosition();
    
    float3 DirLightDir = float3(1, 1, 1);
    
    // shadowRay
    float ShadowRate = 1.0f;
    ShadowRate = 0.0f;
    RayDesc ray;
    ray.Origin = hitPosition;
    float3 vec = g_sceneCB.lightPosition.xyz - hitPosition; //DirLightDir;
    ray.Direction = normalize(vec);
    ray.TMin = 0.001;
    ray.TMax = length(vec); // 10000.0f
    payload.isCollide_Light = 0;
    TraceRay(Scene, RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, ~0, 0, 1, 0, ray, payload);
    ShadowRate = payload.isCollide_Light;
   
    //GetNormal
    float3 triangleNormal;
    {
        // Get the base index of the triangle's first 32 bit index.
        uint indexSizeInBytes = 4;
        uint indicesPerTriangle = 3;
        uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
        uint baseIndex = PrimitiveIndex() * triangleIndexStride;

        // Load up 3 16 bit indices for the triangle.
        const uint3 indices = Indices.Load3(localCB.IBOffset + baseIndex);
        //const uint3 indices = Load3x16BitIndices(baseIndex); // when 16 bit

        // Retrieve corresponding vertex normals for the triangle vertices.
        float3 vertexNormals[3] =
        {
            Vertices[localCB.VBOffset + indices[0]].normal,
            Vertices[localCB.VBOffset + indices[1]].normal,
            Vertices[localCB.VBOffset + indices[2]].normal 
        };

        // Compute the triangle's normal.
        // This is redundant and done for illustration purposes 
        // as all the per-vertex normals are the same and match triangle's normal in this sample. 
        //float3 triangleNormal = HitAttribute(vertexNormals, attr);
        triangleNormal = (vertexNormals[0] + vertexNormals[1] + vertexNormals[2]) / 3;
        float3x3 mat3x3 = WorldToObject3x4();
        triangleNormal = mul(triangleNormal, mat3x3);
    }
    
    payload.ReflectRayStart = hitPosition;
    float3 raydir = WorldRayDirection();
    payload.ReflectRaydir = normalize(reflect(raydir, triangleNormal));
    payload.RelectRate = 0.5f;
    
    //
    
    //float lightW = dot(triangleNormal, DirLightDir);
    float4 diffuseColor = CalculateDiffuseLighting(hitPosition, triangleNormal); //float4(lightW, lightW, lightW, 1);
    float4 color = diffuseColor;
    color.xyz *= ShadowRate;
    payload.color = color;
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
    float4 background = float4(0.0f, 0.0f, 0.0f, 1.0f);
    payload.color = background;
    payload.isCollide_Light = 1.0f;
}