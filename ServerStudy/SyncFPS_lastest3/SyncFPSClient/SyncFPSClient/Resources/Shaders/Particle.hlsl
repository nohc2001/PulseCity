
struct Particle
{
    float3 Position;
    float Life;

    float3 Velocity;
    float Age;
};

// Compute Shader (Particle Update)
// CS
RWStructuredBuffer<Particle> ParticlesRW : register(u0);

cbuffer TimeCB : register(b0)
{
    float dt;
};

[numthreads(256, 1, 1)]
void CS(uint3 id : SV_DispatchThreadID)
{
    uint i = id.x;

    Particle p = ParticlesRW[i];

    if (p.Life <= 0.0f)
    {
        p.Position = float3(0.0f, 5.0f, 0.0f); // y = 5.0
        p.Velocity = float3(
            (frac(sin(i * 12.9898) * 43758.5453) - 0.5f) * 2.0f, // X ÆÛÁü
            6.0f + frac(sin(i * 78.233) * 12345.6789) * 3.0f, // Y »ó½Â
            (frac(sin(i * 93.9898) * 24634.6345) - 0.5f) * 2.0f // Z ÆÛÁü
        );
        p.Life = 2.0f;
        p.Age = 0.0f;
    }

    // ÀÌµ¿
    p.Position += p.Velocity * dt;

    p.Velocity.y -= 3.0f * dt;
    p.Velocity *= 0.98f;

    p.Life -= dt;
    p.Age += dt;

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

    float size = 2.5f; // ºÒ²É Å©±â

    float3 worldPos =
        p.Position +
        (CamRight * offsets[vId].x +
         CamUp * offsets[vId].y) * size;

    VSOut o;
    o.Pos = mul(float4(worldPos, 1), ViewProj);

    float t = saturate(p.Age / p.Life);
    o.Color = lerp(
        float4(1, 1, 0, 1),
        float4(1, 0.2, 0, 0.6f),
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
