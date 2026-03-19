cbuffer VBSize : register(b0)
{
    uint OutputBufferSize;
};

cbuffer cbBoneToLocalWorldInfo : register(b1)
{
    matrix ToLocalMatrixs[128];
};

cbuffer cbWorldInfo : register(b2)
{
    matrix ToWorldMatrixs[128];
};

//48byte (768 바이트 정렬)
struct Vertex
{
    float3 position;
    float u;
    float v;
    float3 normal;
    float3 tangent;
    float extra;
};

struct BoneData
{
    int boneID0;
    float weight0;
    int boneID1;
    float weight1;
    int boneID2;
    float weight2;
    int boneID3;
    float weight3;
};

StructuredBuffer<Vertex> VertexInput : register(t0);
StructuredBuffer<BoneData> BoneInput : register(t1);
RWStructuredBuffer<Vertex> OutVertexBuffer : register(u0, space0);

[numthreads(768, 1, 1)]
void CSMain(int3 n3DispatchThreadID : SV_DispatchThreadID)
{
    if (OutputBufferSize <= n3DispatchThreadID.x)
        return;
    int x = n3DispatchThreadID.x;
    float4x4 mtxVertexToBoneWorld = (float4x4) 0.0f;
    BoneData inputBone = BoneInput[x];
    Vertex inputVertex = VertexInput[x];
    matrix w0 = ToWorldMatrixs[inputBone.boneID0];
    matrix w1 = ToWorldMatrixs[inputBone.boneID1];
    matrix w2 = ToWorldMatrixs[inputBone.boneID2];
    matrix w3 = ToWorldMatrixs[inputBone.boneID3];
    mtxVertexToBoneWorld += inputBone.weight0 * mul(ToLocalMatrixs[inputBone.boneID0], w0);
    mtxVertexToBoneWorld += inputBone.weight1 * mul(ToLocalMatrixs[inputBone.boneID1], w1);
    mtxVertexToBoneWorld += inputBone.weight2 * mul(ToLocalMatrixs[inputBone.boneID2], w2);
    mtxVertexToBoneWorld += inputBone.weight3 * mul(ToLocalMatrixs[inputBone.boneID3], w3);
    OutVertexBuffer[x].position = mul(float4(inputVertex.position, 1.0f), mtxVertexToBoneWorld);
    OutVertexBuffer[x].u = inputVertex.u;
    OutVertexBuffer[x].v = inputVertex.v;
    OutVertexBuffer[x].normal = mul(inputVertex.normal, (float3x3) mtxVertexToBoneWorld).xyz;
    OutVertexBuffer[x].tangent = mul(inputVertex.tangent, (float3x3) mtxVertexToBoneWorld).xyz;
}