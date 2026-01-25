
struct Particle
{
    float3 Position;
    float Life;

    float3 Velocity;
    float Age;

    float Size;
    float Padding;
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
    float sizeAnim = sin(t * 3.14159f);
    float currentSize = p.Size * (0.5f + sizeAnim);

    float2 offsets[6] =
    {
        float2(-1, -1), float2(-1, 1), float2(1, 1),
        float2(-1, -1), float2(1, 1), float2(1, -1)
    };

    float3 worldPos =
        p.Position +
        (CamRight * offsets[vId].x +
         CamUp * offsets[vId].y) * currentSize;

    VSOut o;
    o.Pos = mul(float4(worldPos, 1), ViewProj);

    float4 ColorStart = float4(1.0f, 0.9f, 0.5f, 1.0f);
    float4 ColorMid = float4(1.0f, 0.4f, 0.0f, 1.0f);
    float4 ColorEnd = float4(0.0f, 0.0f, 0.0f, 0.0f);

    if (t < 0.5f)
        o.Color = lerp(ColorStart, ColorMid, t * 2.0f);
    else
        o.Color = lerp(ColorMid, ColorEnd, (t - 0.5f) * 2.0f);


    float2 baseUVs[6] =
    {
        float2(0, 1), float2(0, 0), float2(1, 0),
        float2(0, 1), float2(1, 0), float2(1, 1)
    };
    
    float2 uv = baseUVs[vId];

    uint totalFrames = FrameCols * FrameRows;
    uint currentFrameIndex = (uint) (t * (totalFrames - 1));

    uint col = currentFrameIndex % FrameCols;
    uint row = currentFrameIndex / FrameCols;

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

