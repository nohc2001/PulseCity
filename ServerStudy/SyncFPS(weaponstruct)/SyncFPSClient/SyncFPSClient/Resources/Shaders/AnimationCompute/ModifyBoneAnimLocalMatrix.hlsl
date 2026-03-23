/*
중요한것.
1. 아마 애니메이션 하나당 300KB ~ 600KB 정도 할 건데 이정도면 메모리 문제는 없다.
2. 하지만 GPU는 한 틱에서 이걸 컴퓨트할때 특정 시간에 대해서만 실행하기 때문에 
    같은 시간의 키 데이터는 인접하게 위치시킬 필요가 있다.
*/

struct AnimValue
{
    float4 pos;
    float4 rot;
};

// 쿼터니언으로 벡터 회전시키는 코드 
// AI Code Start
float3 RotateVectorByQuat(float3 v, float4 q)
{
    float3 t = 2.0f * cross(q.xyz, v);
    return v + q.w * t + cross(q.xyz, t);
}
//AI Code End

float4x4 GetRTMatrix(float3 pos, float4 Q)
{
    float4x4 mat = 0;
    float3 right = float3(1, 0, 0);
    float3 up = float3(0, 1, 0);
    float3 look = float3(0, 0, 1);
    mat[0].xyz = RotateVectorByQuat(right, Q);
    mat[1].xyz = RotateVectorByQuat(up, Q);
    mat[2].xyz = RotateVectorByQuat(look, Q);
    mat[3].xyz = pos;
    mat[3].w = 1;
    return mat;
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

cbuffer CBStruct : register(b0)
{
    // 블랜딩할 4개의 애니메이션 각각의 시간
    float4 animTime;
    
    // 블랜딩할 4개의 애니메이션 각각의 최대시간
    float4 MAXTime;
    
    // 블랜딩할 4개의 애니메이션 각각의 블랜딩 가중치
    float4 animWeight;
    
    // Root를 제외한 64개의 Humanoid Bone의 마스킹.
    uint2 animMask0;
    uint2 animMask1;
    uint2 animMask2;
    uint2 animMask3;
    
    // 애니메이션 프레임 레이트
    float frameRate;
};

// TPOS 상태의 각 노드들의 WorldMat
// 64 인덱스 -> 하나의 키임. 해당 64개의 인덱스 내에는 같은 시간임.
StructuredBuffer<AnimValue> Animation0 : register(t0); // t0 t1 t2 t3
StructuredBuffer<AnimValue> Animation1 : register(t1);
StructuredBuffer<AnimValue> Animation2 : register(t2);
StructuredBuffer<AnimValue> Animation3 : register(t3);
StructuredBuffer<int> HumanoidToNodeindex : register(t4);
RWStructuredBuffer<matrix> OutNodeMatrixs : register(u0);

// 왜 뼈는 55개인데 스레드는 32개인가?
// 이건 엔비디아 GPU의 캐시라인이 128바이트인 탓이다. 
// 구조적으로 한 캐시라인에 둘이 쓰면 false shareing이 일어날 수 밖에 없기 때문에
// 한 스레드가 128바이트 쓰기를 당담해야 하고, 서로의 캐시라인을 침범하지 말아야한다.
// 마지막 본은 계산상 생략한다.
[numthreads(32, 1, 1)]
void CSMain(int3 n3DispatchThreadID : SV_DispatchThreadID)
{
    const float4x4 id = float4x4(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1);
    
    int x = n3DispatchThreadID.x;
    int hboneIndex0 = 2 * x;
    int hboneIndex1 = 2 * x + 1;
    int4 keyTime = min(animTime, MAXTime) * frameRate;
    
    float4 CurWeight0 = animWeight;
    float4 CurWeight1 = animWeight;
    uint4 mask = 0;
    if (x < 16)
        mask = uint4(animMask0.x, animMask1.x, animMask2.x, animMask3.x);
    else
        mask = uint4(animMask0.y, animMask1.y, animMask2.y, animMask3.y);
    
    CurWeight0.x *= !!(mask.x & (1 << hboneIndex0));
    CurWeight0.y *= !!(mask.y & (1 << hboneIndex0));
    CurWeight0.z *= !!(mask.z & (1 << hboneIndex0));
    CurWeight0.w *= !!(mask.w & (1 << hboneIndex0));
    CurWeight1.x *= !!(mask.x & (1 << hboneIndex1));
    CurWeight1.y *= !!(mask.y & (1 << hboneIndex1));
    CurWeight1.z *= !!(mask.z & (1 << hboneIndex1));
    CurWeight1.w *= !!(mask.w & (1 << hboneIndex1));
    
    float totalDiv0 = 1.0 / (CurWeight0.x + CurWeight0.y + CurWeight0.z + CurWeight0.w);
    
    AnimValue key00 = Animation0.Load(keyTime.x * 64 + hboneIndex0);
    AnimValue key01 = Animation1.Load(keyTime.y * 64 + hboneIndex0);
    AnimValue key02 = Animation2.Load(keyTime.z * 64 + hboneIndex0);
    AnimValue key03 = Animation3.Load(keyTime.w * 64 + hboneIndex0);
    AnimValue key10 = Animation0.Load(keyTime.x * 64 + hboneIndex1);
    AnimValue key11 = Animation1.Load(keyTime.y * 64 + hboneIndex1);
    AnimValue key12 = Animation2.Load(keyTime.z * 64 + hboneIndex1);
    AnimValue key13 = Animation3.Load(keyTime.w * 64 + hboneIndex1);
    
    AnimValue destV0, destV1;
    destV0.pos = key00.pos * CurWeight0.x;
    destV0.pos += key01.pos * CurWeight0.y;
    destV0.pos += key02.pos * CurWeight0.z;
    destV0.pos += key03.pos * CurWeight0.w;
    destV0.pos *= totalDiv0;
    destV0.rot = BlendQuat4(
        key00.rot, CurWeight0.x,
        key01.rot, CurWeight0.y,
        key02.rot, CurWeight0.z,
        key03.rot, CurWeight0.w);
    
    float totalDiv1 = 1.0 / (CurWeight1.x + CurWeight1.y + CurWeight1.z + CurWeight1.w);
    destV1.pos = key10.pos * CurWeight1.x;
    destV1.pos += key11.pos * CurWeight1.y;
    destV1.pos += key12.pos * CurWeight1.z;
    destV1.pos += key13.pos * CurWeight1.w;
    destV1.pos *= totalDiv1;
    destV1.rot = BlendQuat4(
        key10.rot, CurWeight1.x,
        key11.rot, CurWeight1.y,
        key12.rot, CurWeight1.z,
        key13.rot, CurWeight1.w);

    int nodeindex0 = HumanoidToNodeindex[hboneIndex0];
    if (nodeindex0 >= 0)
        OutNodeMatrixs[nodeindex0] = GetRTMatrix(destV0.pos.xyz, destV0.rot);
    
    int nodeindex1 = HumanoidToNodeindex[hboneIndex1];
    if (nodeindex1 >= 0)
        OutNodeMatrixs[nodeindex1] = GetRTMatrix(destV1.pos.xyz, destV1.rot);
}