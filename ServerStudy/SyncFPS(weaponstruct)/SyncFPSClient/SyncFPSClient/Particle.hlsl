
struct Particle
{
    float3 Position;
    float Life;

    float3 Velocity;
    float Age;

    float Size;
    uint Type;
    uint SpriteIdx;
    float Rotation;
};

// Compute Shader (Particle Update)
// CS
RWStructuredBuffer<Particle> ParticlesRW : register(u0);

cbuffer TimeCB : register(b0)
{
    float dt;
};

float rand(float x)
{
    return frac(sin(x) * 43758.5453);
}

[numthreads(256, 1, 1)]
void FireCS(uint3 id : SV_DispatchThreadID)
{
    uint i = id.x;
    Particle p = ParticlesRW[i];

    if (p.Life <= 0)
    {
        p.Age = 0;

        float3 base = float3(-3, 4, 0);

        float angle = rand(i) * 6.2831853;
        float radius = rand(i + 1);

        float2 dir = float2(cos(angle), sin(angle));

        float upward = 8.0 + rand(i + 2) * 4.0;
        float spread = 2.0 + radius * 2.0;

        p.Position = base;
        p.Velocity = float3(dir.x * spread, upward, dir.y * spread);

        p.Life = 4.5;
        p.Size = 0.08f;
    }

    p.Position += p.Velocity * dt;

    p.Velocity.y -= 9.8 * dt;

    p.Age += dt;
    p.Life -= dt;

    ParticlesRW[i] = p;
}

[numthreads(256, 1, 1)]
void FirePillarCS(uint3 id : SV_DispatchThreadID)
{
    uint i = id.x;
    Particle p = ParticlesRW[i];

    if (p.Life <= 0.0f)
    {
        if (p.Age >= 0.0f)
        {
            float delay = lerp(0.0f, 0.35f, rand(i + 101));
            p.Age = -delay;
        }

        p.Age += dt;
        
        if (p.Age < 0.0f)
        {
            p.Size = 0.0f;
            p.Position = float3(-3, 0.5, 0);
            
            ParticlesRW[i] = p;
            return;
        }

        p.Age = 0.0f;

        float3 base = float3(-3, 0.5, 0);

        float angle = rand(i + 1) * 6.28318f;
        float radius = sqrt(rand(i + 17)) * 0.75f;
        float2 offset = float2(cos(angle), sin(angle)) * radius;

        p.Position = base + float3(offset.x, 0, offset.y);

        p.Velocity = float3(0.0f, lerp(7.5f, 12.0f, rand(i + 3)), 0.0f);
        p.Life = lerp(1.2f, 1.8f, rand(i + 7));
        p.Size = lerp(0.2f, 0.3f, rand(i + 9));
    }

    // 이동
    p.Position += p.Velocity * dt;
    p.Velocity.y -= 9.8f * dt;

    p.Age += dt;
    p.Life -= dt;

    ParticlesRW[i] = p;
}

[numthreads(256, 1, 1)]
void FireRingCS(uint3 id : SV_DispatchThreadID)
{
    uint i = id.x;
    Particle p = ParticlesRW[i];

    float3 centerPos = float3(-3, 1.8, 0);

    if (p.Life <= 0)
    {
        p.Age = 0;

        float angle = rand(i) * 6.28318f;

        float2 dir = float2(cos(angle), sin(angle));
        
        float2 tangent = float2(dir.y, -dir.x);

        float startRadius = 0.5f + rand(i + 1) * 0.5f;
        p.Position = centerPos + float3(dir.x * startRadius, 0, dir.y * startRadius);

        float expandSpeed = 6.0f + rand(i + 2) * 2.0f;
        float rotateSpeed = 6.0f;

        p.Velocity.x = (dir.x * expandSpeed) + (tangent.x * rotateSpeed);
        p.Velocity.y = 0.0f;
        p.Velocity.z = (dir.y * expandSpeed) + (tangent.y * rotateSpeed);

        p.Life = 1.5f + rand(i + 3) * 0.4f; // 수명
        p.Size = 0.1f + rand(i + 4) * 0.1f;
    }

    p.Velocity.x *= 0.98f;
    p.Velocity.z *= 0.98f;

    p.Position += p.Velocity * dt;
    

    p.Age += dt;
    p.Life -= dt;

    ParticlesRW[i] = p;
}

cbuffer MuzzleCB : register(b1)
{
    float4 MuzzlePos; 
    float4 MuzzleDir; 
    float MuzzleBurst; 
    float3 muzzle_pad;
};

[numthreads(256, 1, 1)]
void MuzzleFlashCS(uint3 id : SV_DispatchThreadID)
{
    uint i = id.x;
    Particle p = ParticlesRW[i];

    if (MuzzleBurst > 0.5f && p.Life <= 0.0f)
    {
        float spawnChance = rand(i + MuzzlePos.x + MuzzlePos.y);
        
        if (spawnChance < 0.06f)
        {
            p.Age = 0;
            p.Position = MuzzlePos.xyz;
            
            float typeRand = rand(i * 13.0f);
            
            if (typeRand < 0.1f)
            {
                p.Type = 2;
                p.SpriteIdx = 5; 
                p.Rotation = rand(i * 7.0f) * 6.28318f; 
                p.Size = 0.2f + rand(i) * 0.15f;
                p.Life = 0.03f + rand(i) * 0.02f; 
                
                p.Velocity = MuzzleDir.xyz * 2.0f;
            }
            else if (typeRand < 0.4f)
            {
                p.Type = 0;
                p.SpriteIdx = 10; 
                p.Rotation = rand(i * 3.0f) * 6.28318f;
                p.Size = 0.08f + rand(i) * 0.05f; 
                p.Life = 0.15f + rand(i) * 0.1f; 
                
                float3 randDir = float3(rand(i) - 0.5, rand(i + 1) - 0.5, rand(i + 2) - 0.5);
                p.Velocity = normalize(MuzzleDir.xyz * 1.5f + randDir) * (15.0f + rand(i) * 15.0f);
            }
            else
            {
                p.Type = 1;
                p.SpriteIdx = 0; 
                p.Rotation = 0.0f;
                p.Size = 0.015f + rand(i) * 0.02f; 
                p.Life = 0.2f + rand(i) * 0.3f; 
                
                float spread = MuzzleDir.w * 2.5f;
                float3 randDir = float3(rand(i) - 0.5, rand(i + 1) - 0.5, rand(i + 2) - 0.5) * spread;
                p.Velocity = normalize(MuzzleDir.xyz * 4.0f + randDir) * (50.0f + rand(i) * 80.0f);
            }
        }
    }

    if (p.Life > 0.0f)
    {
        p.Age += dt;
        p.Life -= dt;
        
        if (p.Type == 1) 
        {
            p.Velocity.y -= 30.0f * dt; 
            p.Velocity *= 0.95f; 
            p.Position += p.Velocity * dt;
        }
        else if (p.Type == 0) 
        {
            p.Velocity *= 0.85f;
            p.Position += p.Velocity * dt;
            p.Position.y += 0.5f * dt; 
        }
        else if (p.Type == 2) 
        {
            p.Position += p.Velocity * dt;
        }
    }
    else
    {
        p.Size = 0.0f;
        p.Life = -1.0f;
        p.Type = 0;
    }

    ParticlesRW[i] = p;
}

cbuffer TracerCB : register(b1)
{
    float4 TracerMuzzlePos;
    float4 TracerDir;
    float4 TracerParams; // x:Speed, y:Life, z:Size, w:Count
};

[numthreads(256, 1, 1)]
void TracerCS(uint3 id : SV_DispatchThreadID)
{
    uint i = id.x;
    Particle p = ParticlesRW[i];

    float rawW = TracerParams.w;
    
    // [1] 생성 로직
    if (rawW >= 0.0f)
    {
        uint startIndex = (uint) rawW;
        uint count = (uint) ((rawW - startIndex) * 10000.0f + 0.5f);

        if (count > 0 && i >= startIndex && i < startIndex + count)
        {
            p.Age = 0;
            p.Position = TracerMuzzlePos.xyz;
            
            // 탄퍼짐(Spread)
            float spread = TracerDir.w;
            float3 randDir = float3(rand(i) - 0.5, rand(i + 1) - 0.5, rand(i + 2) - 0.5) * spread;
            
            p.Velocity = normalize(TracerDir.xyz + randDir) * TracerParams.x;
            p.Life = TracerParams.y;
            p.Size = TracerParams.z;
            
            ParticlesRW[i] = p;
            return;
        }
    }

    // [2] 업데이트 (탄도학 및 물리 법칙 적용)
    if (p.Life > 0.0f)
    {
        p.Age += dt;
        p.Life -= dt;
        
        // 1. 공기 저항 (Drag): 총알은 날아가면서 점점 속도를 잃습니다.
        // dt에 곱해서 프레임 독립적으로 만듭니다.
        p.Velocity *= (1.0f - 0.5f * dt);
        
        // 2. 중력 (Gravity - Bullet Drop): 장거리 사격 시 총알이 아래로 미세하게 떨어집니다.
        p.Velocity.y -= 9.8f * dt;
        
        // 위치 업데이트
        p.Position += p.Velocity * dt;
        
        // 3. 예광탄 연소 (Burn-out): 예광탄은 빛을 내며 타기 때문에 수명이 다해갈수록 작아집니다.
        // 처음 부여받은 전체 수명(Age + Life) 대비 남은 생명력의 비율을 구합니다.
        float maxLife = p.Age + p.Life;
        float lifeRatio = p.Life / maxLife;
        
        // 수명이 20% 이하로 남았을 때부터 크기가 스르륵 줄어들며 자연스럽게 소멸합니다.
        p.Size = TracerParams.z * saturate(lifeRatio * 5.0f);
    }
    else
    {
        p.Size = 0.0f;
        p.Life = -1.0f;
    }

    ParticlesRW[i] = p;
}

// Vertex Shader (SV_VertexID)
// VS
StructuredBuffer<Particle> Particles : register(t0);

cbuffer CameraCB : register(b0)
{
    float4x4 ViewProj;
    float3 CamRight;
    float pad0;
    float3 CamUp;
};

struct VSOut
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
};

VSOut VS(uint id : SV_VertexID)
{
    uint quadId = id / 6;
    uint vId = id % 6;

    Particle p = Particles[quadId];

    uint FrameCols = 6;
    uint FrameRows = 6;

    float t = saturate(p.Age / max(p.Life, 0.001));
    float currentSize = p.Size * (1.0f - t * 0.2f);

    float2 offsets[6] =
    {
        float2(-1, -1), float2(-1, 1), float2(1, 1),
        float2(-1, -1), float2(1, 1), float2(1, -1)
    };

    float3 worldPos = p.Position;
    float3 velocity = p.Velocity;
    float speed = length(velocity);

    float cosR = cos(p.Rotation);
    float sinR = sin(p.Rotation);
    float2 rotatedOffset = float2(
        offsets[vId].x * cosR - offsets[vId].y * sinR,
        offsets[vId].x * sinR + offsets[vId].y * cosR
    );

    if (p.Type == 0) 
    {
        float expandSize = currentSize * (1.0f + t);
        worldPos += (CamRight * rotatedOffset.x + CamUp * rotatedOffset.y) * expandSize;
    }
    else if (p.Type == 1)
    {
        if (speed > 10.0f) 
        {
            float3 vDir = normalize(velocity);
            float3 sideDir = normalize(cross(vDir, CamUp));
            
            float bulletStretch = min(speed * 0.002f, 0.7f);
            
            worldPos += (sideDir * offsets[vId].x * currentSize * 0.5f) +
                        (vDir * offsets[vId].y * (currentSize + bulletStretch));
        }
        else
        {
            worldPos += (CamRight * rotatedOffset.x + CamUp * rotatedOffset.y) * currentSize;
        }
    }
    else if (p.Type == 2) 
    {
        float flashScale = sin(t * 3.14159f) * 1.5f;
        
        worldPos += (CamRight * rotatedOffset.x + CamUp * rotatedOffset.y) * (currentSize * flashScale);
    }
    else if (p.Type == 3) 
    {
        float3 right = float3(1, 0, 0);
        float3 forward = float3(0, 0, 1);
        worldPos += (right * rotatedOffset.x + forward * rotatedOffset.y) * currentSize;
    }

    VSOut o;
    o.Pos = mul(float4(worldPos, 1), ViewProj);

    float4 ColorStart = float4(2.0f, 1.8f, 1.0f, 1.0f);
    float4 ColorMid = float4(1.0f, 0.5f, 0.1f, 1.0f);
    float4 ColorEnd = float4(0.0f, 0.0f, 0.0f, 0.0f);

    if (t < 0.2f)
        o.Color = lerp(ColorStart, ColorMid, t * 5.0f);
    else
        o.Color = lerp(ColorMid, ColorEnd, (t - 0.2f) * 1.25f);

    float2 baseUVs[6] = { float2(0, 1), float2(0, 0), float2(1, 0), float2(0, 1), float2(1, 0), float2(1, 1) };
    float2 uv = baseUVs[vId];

    uint col = p.SpriteIdx % FrameCols;
    uint row = p.SpriteIdx / FrameCols;

    uv.x = (uv.x / (float) FrameCols) + ((float) col / (float) FrameCols);
    uv.y = (uv.y / (float) FrameRows) + ((float) row / (float) FrameRows);

    o.UV = uv;
    return o;
}

// Pixel Shader
// PS
Texture2D FireTex : register(t1);
SamplerState LinearSampler : register(s0);

float4 PS(VSOut i) : SV_Target
{
    float4 tex = FireTex.Sample(LinearSampler, i.UV);
    return tex * i.Color;
}

