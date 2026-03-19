/*
중요한것.
1. 아마 애니메이션 하나당 300KB ~ 600KB 정도 할 건데 이정도면 메모리 문제는 없다.
2. 하지만 GPU는 한 틱에서 이걸 컴퓨트할때 특정 시간에 대해서만 실행하기 때문에 
    같은 시간의 키 데이터는 인접하게 위치시킬 필요가 있다.
*/

cbuffer CBStruct : register(b0)
{
    // 블랜딩할 4개의 애니메이션 각각의 시간
    float4 animTime;
    
    // 블랜딩할 4개의 애니메이션 각각의 블랜딩 가중치
    float4 animWeight;
    
    // Root를 제외한 64개의 Humanoid Bone의 마스킹.
    uint2 animMask0;
    uint2 animMask1;
    uint2 animMask2;
    uint2 animMask3;
};

struct AnimValue
{
    float4 pos;
    float4 rot;
};

struct AnimKey
{
    // 같은 시간의 키 데이터를 인접하게 위치시킨다. 
    // (한 SM에서 캐시라인을 용이하게 불러오도록.)
    AnimValue valueArr[65];
};

// 쿼터니언으로 벡터 회전시키는 코드 AI Code Start
float3 RotateVectorByQuat(float3 v, float4 q)
{
    float3 t = 2.0f * cross(q.xyz, v);
    return v + q.w * t + cross(q.xyz, t);
}
//AI Code End

float4x4 GetRTMatrix(float3 pos, float4 Q)
{
    float4x4 mat;
    float3 right = float3(1, 0, 0);
    float3 up = float3(0, 1, 0);
    float3 look = float3(0, 0, 1);
    mat[0].xyz = RotateVectorByQuat(right, Q);
    mat[1].xyz = RotateVectorByQuat(up, Q);
    mat[2].xyz = RotateVectorByQuat(look, Q);
    mat[0].xyz = pos;
    mat[0].w = 1;
    return transpose(mat);
}

//쿼터니언 가중치 보간 AI Code Start
float4 BlendQuat4(float4 q0, float w0,
                  float4 q1, float w1,
                  float4 q2, float w2,
                  float4 q3, float w3)
{
    // 반대 방향 보정
    if (dot(q1, q0) < 0)
        q1 = -q1;
    if (dot(q2, q0) < 0)
        q2 = -q2;
    if (dot(q3, q0) < 0)
        q3 = -q3;

    float4 blended =
        q0 * w0 +
        q1 * w1 +
        q2 * w2 +
        q3 * w3;

    return normalize(blended);
}
// AI Code End

// TPOS 상태의 각 노드들의 WorldMat
StructuredBuffer<matrix> DefaultNodeTr : register(t0);
StructuredBuffer<int> RetargetingIndexToNode : register(t1);
StructuredBuffer<AnimKey> Animation0 : register(t1);
StructuredBuffer<AnimKey> Animation1 : register(t2);
StructuredBuffer<AnimKey> Animation2 : register(t3);
StructuredBuffer<AnimKey> Animation3 : register(t4);
RWStructuredBuffer<matrix> OutNodeMatrixs : register(u0);

// 왜 뼈는 65개인데 스레드는 32개인가?
// 이건 엔비디아 GPU의 캐시라인이 128바이트인 탓이다. 
// 구조적으로 한 캐시라인에 둘이 쓰면 false shareing이 일어날 수 밖에 없기 때문에
// 한 스레드가 128바이트 쓰기를 당담해야 하고, 서로의 캐시라인을 침범하지 말아야한다.
// 마지막 본은 계산상 생략한다.
[numthreads(32, 1, 1)]
void CSMain(int3 n3DispatchThreadID : SV_DispatchThreadID)
{
    int x = n3DispatchThreadID.x;
    int hboneIndex0 = 2 * x;
    int hboneIndex1 = 2 * x + 1;
    
    float4 CurWeight0 = animWeight;
    float4 CurWeight1 = animWeight;
    uint4 mask = 0;
    if (x < 16)
        mask = uint4(animMask0.x, animMask1.x, animMask2.x, animMask3.x);
    else
        mask = uint4(animMask0.y, animMask1.y, animMask2.y, animMask3.y);
    
    CurWeight0.x *= !!(mask.x & (1 << hboneIndex0) == 0);
    CurWeight0.y *= !!(mask.y & (1 << hboneIndex0) == 0);
    CurWeight0.z *= !!(mask.z & (1 << hboneIndex0) == 0);
    CurWeight0.w *= !!(mask.w & (1 << hboneIndex0) == 0);
    CurWeight1.x *= !!(mask.x & (1 << hboneIndex1) == 0);
    CurWeight1.y *= !!(mask.y & (1 << hboneIndex1) == 0);
    CurWeight1.z *= !!(mask.z & (1 << hboneIndex1) == 0);
    CurWeight1.w *= !!(mask.w & (1 << hboneIndex1) == 0);
    
    float totalDiv0 = 1.0 / (CurWeight0.x + CurWeight0.y + CurWeight0.z + CurWeight0.w);
    AnimKey key00 = Animation0.Load(hboneIndex0);
    AnimKey key01 = Animation1.Load(hboneIndex0);
    AnimKey key02 = Animation2.Load(hboneIndex0);
    AnimKey key03 = Animation3.Load(hboneIndex0);
    AnimKey key10 = Animation0.Load(hboneIndex1);
    AnimKey key11 = Animation1.Load(hboneIndex1);
    AnimKey key12 = Animation2.Load(hboneIndex1);
    AnimKey key13 = Animation3.Load(hboneIndex1);
    int nodeindex = RetargetingIndexToNode.Load(x);
    AnimValue destV0, destV1;
    destV0.pos += key00.valueArr[x].pos * CurWeight0.x;
    destV0.pos += key01.valueArr[x].pos * CurWeight0.y;
    destV0.pos += key02.valueArr[x].pos * CurWeight0.z;
    destV0.pos += key03.valueArr[x].pos * CurWeight0.w;
    destV0.pos *= totalDiv0;
    destV0.rot = BlendQuat4(
        key00.valueArr[x].rot, CurWeight0.x,
        key01.valueArr[x].rot, CurWeight0.y,
        key02.valueArr[x].rot, CurWeight0.z,
        key03.valueArr[x].rot, CurWeight0.w);
    
    float totalDiv1 = 1.0 / (CurWeight1.x + CurWeight1.y + CurWeight1.z + CurWeight1.w);
    destV1.pos += key10.valueArr[x].pos * CurWeight1.x;
    destV1.pos += key11.valueArr[x].pos * CurWeight1.y;
    destV1.pos += key12.valueArr[x].pos * CurWeight1.z;
    destV1.pos += key13.valueArr[x].pos * CurWeight1.w;
    destV1.pos *= totalDiv1;
    destV1.rot = BlendQuat4(
        key10.valueArr[x].rot, CurWeight1.x,
        key11.valueArr[x].rot, CurWeight1.y,
        key12.valueArr[x].rot, CurWeight1.z,
        key13.valueArr[x].rot, CurWeight1.w);

    OutNodeMatrixs[hboneIndex0] = GetRTMatrix(destV0.pos.xyz, destV0.rot);
    OutNodeMatrixs[hboneIndex1] = GetRTMatrix(destV1.pos.xyz, destV1.rot);
}