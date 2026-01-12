
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

        p.Life = 2.5;
        p.Size = 0.03;
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
        p.Life = lerp(0.8f, 1.4f, rand(i + 7));
        p.Size   = lerp(0.02f, 0.08f, rand(i + 9)); 
    }

    // ÀÌµ¿
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

    if (p.Life <= 0)
    {
        p.Age = 0;

        float angle = rand(i) * 6.28318;
        float2 dir = float2(cos(angle), sin(angle));

        float3 base = float3(-3, 0.5, 0);

        p.Position = base;
        p.Velocity = float3(dir.x * 8, 0.5, dir.y * 8);

        p.Life = 1.5;
        p.Size = 0.025;
    }

    p.Position += p.Velocity * dt;
   // p.Velocity.y -= 3.5 * dt;

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
};

VSOut VS(uint id : SV_VertexID)
{
    uint quadId = id / 6;
    uint vId = id % 6;

    Particle p = Particles[quadId];

    float2 offsets[6] =
    {
        float2(-1, -1), float2(-1, 1), float2(1, 1),
        float2(-1, -1), float2(1, 1), float2(1, -1)
    };

    float3 worldPos =
        p.Position +
        (CamRight * offsets[vId].x +
         CamUp * offsets[vId].y) * p.Size;

    VSOut o;
    o.Pos = mul(float4(worldPos, 1), ViewProj);

    float t = saturate(p.Age / max(p.Life, 0.001));

    // ºÒ²É »ö
    o.Color = lerp(
        float4(1, 1, 0, 1),
        float4(1, 0.2, 0, 0),
        t
    );

    return o;
}

// Pixel Shader
// PS
float4 PS(VSOut i) : SV_Target
{
    return i.Color;
}
