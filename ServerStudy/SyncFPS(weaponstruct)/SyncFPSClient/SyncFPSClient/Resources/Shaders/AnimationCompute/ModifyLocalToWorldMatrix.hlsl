// 이 셰이더에는 또하나의 최적화 가능성이 있다.
// 그건 바로 노드들의 Toworld 행렬은 모두 같은데 같은 계산을 게속 하고 있다.
// 비효율이고 나중에 애니메이션 병목이 생길시에 BoneToWorldMatrix를 많이 받아 여러 SkinMesh에 동시적용하게 만들어야 함.

cbuffer WorldMat : register(b0)
{
    float4x4 world;
};

// 현재 노드들의 WorldMat // ModifyBoneAnimLocalMatrix.hlsl 의 출력값.
StructuredBuffer<matrix> LocalMatrixs : register(t0);

// TPOS 상태의 노드들의 WorldMat
StructuredBuffer<matrix> TPOSLocalTr : register(t1);

// Nodeindex. 부모를 가리키는 인덱스
StructuredBuffer<int> toParent : register(t2);

// Nodeindex. 부모를 가리키는 인덱스
StructuredBuffer<int> NodeToBoneIndex : register(t3);

// 실제 스킨메쉬렌더에 쓰일 BoneToWorldMatrix
RWStructuredBuffer<matrix> BoneToWorldMatrix : register(u0);

groupshared float4x4 sharedLocalMatrixs[128];
groupshared int SharedtoParent[128];

[numthreads(64, 1, 1)]
void CSMain(int3 n3DispatchThreadID : SV_DispatchThreadID)
{
    int x = n3DispatchThreadID.x;
    int nodeindex0 = 2 * x;
    int nodeindex1 = 2 * x + 1;
    
    if (nodeindex0 == 0)
    {
        sharedLocalMatrixs[nodeindex0] = world;
    }
    else sharedLocalMatrixs[nodeindex0] = mul(LocalMatrixs[nodeindex0], TPOSLocalTr[nodeindex0]);
    sharedLocalMatrixs[nodeindex1] = mul(LocalMatrixs[nodeindex1], TPOSLocalTr[nodeindex1]);
    SharedtoParent[nodeindex0] = toParent[nodeindex0];
    SharedtoParent[nodeindex1] = toParent[nodeindex1];
    GroupMemoryBarrierWithGroupSync();
    
    int index0 = nodeindex0;
    int index1 = nodeindex1;
    
    const float4x4 id = float4x4(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1);
    
    float4x4 mat0 = sharedLocalMatrixs[index0];
    float4x4 temp0;
    float4x4 mat1 = sharedLocalMatrixs[index1];
    float4x4 temp1;
    
    // 노드로 바꾸어서 반복이 충분치 않을 수도 있다. 근데 대부분 잘 될것 같음.
    [unroll(16)]
    for (int i = 0; i < 16; ++i)
    {
        //0
        index0 = SharedtoParent[index0];
        index1 = SharedtoParent[index1];
        
        int isNeg0 = !!(index0 < 0);
        int safeIndex0 = max(index0, 0);
        temp0 = isNeg0 * id + (1 - isNeg0) * sharedLocalMatrixs[safeIndex0];
        
        int isNeg1 = !!(index1 < 0);
        int safeIndex1 = max(index1, 0);
        temp1 = isNeg1 * id + (1 - isNeg1) * sharedLocalMatrixs[safeIndex1];
        
        mat0 = mul(mat0, temp0);
        mat1 = mul(mat1, temp1);
        
        index0 = safeIndex0;
        index1 = safeIndex1;
    }
    
    int boneindex0 = NodeToBoneIndex[nodeindex0];
    if (boneindex0 >= 0)
        BoneToWorldMatrix[boneindex0] = mat0;
    
    int boneindex1 = NodeToBoneIndex[nodeindex1];
    if (boneindex1 >= 0)
        BoneToWorldMatrix[boneindex1] = mat1;
}