struct Particle
{
    float3 Position;
    float Age;

    float3 Velocity;
    float LifeTime;

    float4 StartColor;
    float4 EndColor;

    float StartSize;
    float EndSize;
    float Rotation;
    float RotationSpeed;

    float Drag;
    float GravityScale;
    float Stretch;
    float CollisionRadius;

    uint Flags;
    uint RandomSeed;
    uint FrameIndex;
    uint FrameCount;
};

RWStructuredBuffer<Particle> ParticlesRW : register(u0);
StructuredBuffer<Particle> Particles : register(t0);

cbuffer TimeCB : register(b0)
{
    float dt;
};

cbuffer CameraCB : register(b0)
{
    float4x4 ViewProj;
    float3 CamRight;
    float pad0;
    float3 CamUp;
};

static const uint PARTICLE_FLAG_COLLIDE_GROUND = 1u;
static const uint PARTICLE_FLAG_ADDITIVE = 2u;
static const uint PARTICLE_FLAG_LOOPING = 4u;

float rand(float x)
{
    return frac(sin(x) * 43758.5453);
}

float randFromSeed(uint seed, float salt)
{
    return rand((float)seed + salt);
}

float3 randomSphere(uint seed, float salt)
{
    float angle = randFromSeed(seed, salt) * 6.2831853f;
    float z = randFromSeed(seed, salt + 11.73f) * 2.0f - 1.0f;
    float radius = sqrt(saturate(1.0f - z * z));
    return float3(cos(angle) * radius, z, sin(angle) * radius);
}

float3 randomUnitCircle(uint seed, float salt)
{
    float angle = randFromSeed(seed, salt) * 6.2831853f;
    return float3(cos(angle), 0.0f, sin(angle));
}

float3 turbulence(float3 position, float phase)
{
    float n1 = rand(position.x * 0.73f + position.y * 1.13f + position.z * 0.37f + phase);
    float n2 = rand(position.z * 0.91f - position.x * 0.41f + position.y * 0.57f + phase * 1.7f);
    float n3 = rand(position.y * 0.63f + position.x * 0.27f - position.z * 0.82f + phase * 2.3f);
    return (float3(n1, n2, n3) * 2.0f - 1.0f);
}

float3 vortexForce(float3 position, float3 center, float strength)
{
    float3 radial = position - center;
    float2 planar = float2(radial.x, radial.z);
    float planarLength = max(length(planar), 0.001f);
    float2 tangent = float2(-planar.y, planar.x) / planarLength;
    return float3(tangent.x, 0.0f, tangent.y) * strength;
}

void resetParticleCommon(
    inout Particle p,
    float lifeTime,
    float4 startColor,
    float4 endColor,
    float startSize,
    float endSize,
    float rotation,
    float rotationSpeed,
    float drag,
    float gravityScale,
    float stretch,
    float collisionRadius,
    uint flags,
    uint frameCount)
{
    p.Age = 0.0f;
    p.LifeTime = lifeTime;
    p.StartColor = startColor;
    p.EndColor = endColor;
    p.StartSize = startSize;
    p.EndSize = endSize;
    p.Rotation = rotation;
    p.RotationSpeed = rotationSpeed;
    p.Drag = drag;
    p.GravityScale = gravityScale;
    p.Stretch = stretch;
    p.CollisionRadius = collisionRadius;
    p.Flags = flags;
    p.FrameIndex = 0;
    p.FrameCount = max(frameCount, 1u);
}

void simulateParticle(
    inout Particle p,
    float3 forceField,
    float turbulenceAmount,
    float groundY,
    float bounce)
{
    float lifeT = saturate(p.Age / max(p.LifeTime, 0.001f));
    float agePhase = p.Age * 3.1f + (float)p.RandomSeed * 0.01f;
    float3 noiseForce = turbulence(p.Position, agePhase) * turbulenceAmount * (1.0f - lifeT * 0.65f);
    float3 gravity = float3(0.0f, -9.81f * p.GravityScale, 0.0f);

    p.Velocity += (forceField + gravity + noiseForce) * dt;
    p.Velocity *= max(0.0f, 1.0f - p.Drag * dt);
    p.Position += p.Velocity * dt;
    p.Rotation += p.RotationSpeed * dt;
    p.Age += dt;

    if ((p.Flags & PARTICLE_FLAG_COLLIDE_GROUND) != 0u)
    {
        float floorY = groundY + p.CollisionRadius;
        if (p.Position.y < floorY)
        {
            p.Position.y = floorY;
            if (p.Velocity.y < 0.0f)
            {
                p.Velocity.y = -p.Velocity.y * bounce;
                p.Velocity.xz *= 0.72f;
            }
        }
    }

    float frameT = saturate(p.Age / max(p.LifeTime, 0.001f));
    p.FrameIndex = min((uint)(frameT * (p.FrameCount - 1)), p.FrameCount - 1);
}

[numthreads(256, 1, 1)]
void FireCS(uint3 id : SV_DispatchThreadID)
{
    uint i = id.x;
    Particle p = ParticlesRW[i];

    if (p.Age >= p.LifeTime)
    {
        float3 base = float3(-3.0f, 4.0f, 0.0f);
        float angle = randFromSeed(p.RandomSeed, 1.0f) * 6.2831853f;
        float radius = randFromSeed(p.RandomSeed, 2.0f);
        float2 dir = float2(cos(angle), sin(angle));

        float upward = 8.5f + randFromSeed(p.RandomSeed, 3.0f) * 4.8f;
        float spread = 2.6f + radius * 2.6f;

        p.Position = base + float3(dir.x * radius * 0.18f, randFromSeed(p.RandomSeed, 4.0f) * 0.10f, dir.y * radius * 0.18f);
        p.Velocity = float3(dir.x * spread, upward, dir.y * spread);
        resetParticleCommon(
            p,
            4.9f,
            float4(1.0f, 0.9f, 0.5f, 1.0f),
            float4(0.0f, 0.0f, 0.0f, 0.0f),
            0.105f + randFromSeed(p.RandomSeed, 5.0f) * 0.025f,
            0.02f,
            0.0f,
            0.0f,
            0.0f,
            1.0f,
            0.0f,
            0.0f,
            PARTICLE_FLAG_ADDITIVE | PARTICLE_FLAG_LOOPING,
            36u);
    }

    p.Position += p.Velocity * dt;
    p.Velocity.y -= 9.8f * dt;
    p.Velocity.x *= 0.992f;
    p.Velocity.z *= 0.992f;
    p.Age += dt;
    if (p.Age > p.LifeTime)
    {
        p.Age = p.LifeTime;
    }
    ParticlesRW[i] = p;
}

[numthreads(256, 1, 1)]
void FirePillarCS(uint3 id : SV_DispatchThreadID)
{
    uint i = id.x;
    Particle p = ParticlesRW[i];

    if (p.Age >= p.LifeTime)
    {
        float3 base = float3(-3.0f, 0.5f, 0.0f);
        float angle = randFromSeed(p.RandomSeed, 101.0f) * 6.2831853f;
        float radius = sqrt(randFromSeed(p.RandomSeed, 102.0f)) * 0.75f;
        float2 offset = float2(cos(angle), sin(angle)) * radius;

        p.Position = base + float3(offset.x, 0.0f, offset.y);
        p.Velocity = float3(offset.x * 0.55f, lerp(7.8f, 12.8f, randFromSeed(p.RandomSeed, 103.0f)), offset.y * 0.55f);
        resetParticleCommon(
            p,
            lerp(1.25f, 1.95f, randFromSeed(p.RandomSeed, 104.0f)),
            float4(1.0f, 0.9f, 0.5f, 1.0f),
            float4(0.0f, 0.0f, 0.0f, 0.0f),
            lerp(0.22f, 0.34f, randFromSeed(p.RandomSeed, 105.0f)),
            0.05f,
            0.0f,
            0.0f,
            0.0f,
            1.0f,
            0.0f,
            0.0f,
            PARTICLE_FLAG_ADDITIVE | PARTICLE_FLAG_LOOPING,
            36u);
    }

    float2 toCenter = float2(-3.0f - p.Position.x, -p.Position.z);
    p.Velocity.x += toCenter.x * 0.85f * dt;
    p.Velocity.z += toCenter.y * 0.85f * dt;
    p.Position += p.Velocity * dt;
    p.Velocity.y -= 9.8f * dt;
    p.Age += dt;
    if (p.Age > p.LifeTime)
    {
        p.Age = p.LifeTime;
    }
    ParticlesRW[i] = p;
}

[numthreads(256, 1, 1)]
void FireRingCS(uint3 id : SV_DispatchThreadID)
{
    uint i = id.x;
    Particle p = ParticlesRW[i];
    float3 centerPos = float3(-3.0f, 1.8f, 0.0f);

    if (p.Age >= p.LifeTime)
    {
        float angle = randFromSeed(p.RandomSeed, 201.0f) * 6.2831853f;
        float2 dir = float2(cos(angle), sin(angle));
        float2 tangent = float2(dir.y, -dir.x);

        float startRadius = 0.5f + randFromSeed(p.RandomSeed, 202.0f) * 0.5f;
        p.Position = centerPos + float3(dir.x * startRadius, 0.0f, dir.y * startRadius);

        float expandSpeed = 6.0f + randFromSeed(p.RandomSeed, 203.0f) * 2.0f;
        float rotateSpeed = 6.0f;

        p.Velocity.x = dir.x * expandSpeed + tangent.x * rotateSpeed;
        p.Velocity.y = 0.0f;
        p.Velocity.z = dir.y * expandSpeed + tangent.y * rotateSpeed;

        resetParticleCommon(
            p,
            1.65f + randFromSeed(p.RandomSeed, 204.0f) * 0.45f,
            float4(1.0f, 0.9f, 0.5f, 1.0f),
            float4(0.0f, 0.0f, 0.0f, 0.0f),
            0.13f + randFromSeed(p.RandomSeed, 205.0f) * 0.08f,
            0.03f,
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            PARTICLE_FLAG_ADDITIVE | PARTICLE_FLAG_LOOPING,
            36u);
    }

    p.Velocity.x *= 0.98f;
    p.Velocity.z *= 0.98f;
    p.Position += p.Velocity * dt;
    p.Age += dt;
    if (p.Age > p.LifeTime)
    {
        p.Age = p.LifeTime;
    }
    ParticlesRW[i] = p;
}

[numthreads(256, 1, 1)]
void ElectricArcCS(uint3 id : SV_DispatchThreadID)
{
    uint i = id.x;
    Particle p = ParticlesRW[i];
    float3 centerPos = float3(-1.6f, 1.85f, 0.0f);

    if (p.Age >= p.LifeTime)
    {
        float orbit = randFromSeed(p.RandomSeed, 301.0f) * 6.2831853f;
        float radius = 0.18f + randFromSeed(p.RandomSeed, 302.0f) * 0.42f;
        float height = randFromSeed(p.RandomSeed, 303.0f) * 1.9f;
        float2 ring = float2(cos(orbit), sin(orbit)) * radius;

        p.Position = centerPos + float3(ring.x, height, ring.y);
        p.Velocity = float3(-ring.y * (2.4f + randFromSeed(p.RandomSeed, 304.0f) * 1.8f),
                            0.9f + randFromSeed(p.RandomSeed, 305.0f) * 2.4f,
                            ring.x * (2.4f + randFromSeed(p.RandomSeed, 306.0f) * 1.8f));

        resetParticleCommon(
            p,
            0.38f + randFromSeed(p.RandomSeed, 307.0f) * 0.28f,
            float4(0.58f, 0.88f, 1.65f, 0.95f),
            float4(0.05f, 0.14f, 0.35f, 0.0f),
            0.20f + randFromSeed(p.RandomSeed, 308.0f) * 0.10f,
            0.05f,
            orbit,
            -2.8f + randFromSeed(p.RandomSeed, 309.0f) * 5.6f,
            2.4f + randFromSeed(p.RandomSeed, 310.0f) * 1.4f,
            0.0f,
            0.75f,
            0.0f,
            PARTICLE_FLAG_ADDITIVE | PARTICLE_FLAG_LOOPING,
            36u);
    }

    float3 arcPull = (centerPos - p.Position) * float3(1.85f, 0.15f, 1.85f);
    simulateParticle(p, float3(0.0f, 2.6f, 0.0f) + arcPull, 1.85f, 0.0f, 0.0f);
    ParticlesRW[i] = p;
}

[numthreads(256, 1, 1)]
void ElectricBurstCS(uint3 id : SV_DispatchThreadID)
{
    uint i = id.x;
    Particle p = ParticlesRW[i];
    float3 centerPos = float3(-0.7f, 1.4f, 0.0f);

    if (p.Age >= p.LifeTime)
    {
        float angle = randFromSeed(p.RandomSeed, 401.0f) * 6.2831853f;
        float radius = 0.08f + randFromSeed(p.RandomSeed, 402.0f) * 0.16f;
        float2 dir = float2(cos(angle), sin(angle));

        p.Position = centerPos + float3(dir.x * radius, randFromSeed(p.RandomSeed, 403.0f) * 0.28f, dir.y * radius);
        p.Velocity = float3(dir.x * (3.8f + randFromSeed(p.RandomSeed, 404.0f) * 2.1f),
                            0.6f + randFromSeed(p.RandomSeed, 405.0f) * 1.2f,
                            dir.y * (3.8f + randFromSeed(p.RandomSeed, 406.0f) * 2.1f));

        resetParticleCommon(
            p,
            0.24f + randFromSeed(p.RandomSeed, 407.0f) * 0.16f,
            float4(0.65f, 0.95f, 1.90f, 1.0f),
            float4(0.10f, 0.18f, 0.45f, 0.0f),
            0.12f + randFromSeed(p.RandomSeed, 408.0f) * 0.08f,
            0.03f,
            angle,
            -4.0f + randFromSeed(p.RandomSeed, 409.0f) * 8.0f,
            3.6f + randFromSeed(p.RandomSeed, 410.0f) * 1.2f,
            0.0f,
            1.15f,
            0.0f,
            PARTICLE_FLAG_ADDITIVE | PARTICLE_FLAG_LOOPING,
            36u);
    }

    float3 snapBack = (centerPos - p.Position) * float3(0.55f, 0.08f, 0.55f);
    float3 swirl = vortexForce(p.Position, centerPos, 2.6f);
    simulateParticle(p, float3(0.0f, 1.2f, 0.0f) + snapBack + swirl, 2.15f, 0.0f, 0.0f);
    ParticlesRW[i] = p;
}

[numthreads(256, 1, 1)]
void EmberShowerCS(uint3 id : SV_DispatchThreadID)
{
    uint i = id.x;
    Particle p = ParticlesRW[i];
    float3 centerPos = float3(-3.0f, 5.6f, 0.0f);

    if (p.Age >= p.LifeTime)
    {
        float3 spread = randomSphere(p.RandomSeed, 501.0f);
        spread.y = abs(spread.y);

        p.Position = centerPos + float3(spread.x * 0.42f, spread.y * 0.16f, spread.z * 0.42f);
        p.Velocity = float3(spread.x * 1.4f,
                            1.1f + randFromSeed(p.RandomSeed, 502.0f) * 1.9f,
                            spread.z * 1.4f);

        resetParticleCommon(
            p,
            0.95f + randFromSeed(p.RandomSeed, 503.0f) * 0.55f,
            float4(1.35f, 0.55f, 0.12f, 1.0f),
            float4(0.18f, 0.07f, 0.03f, 0.0f),
            0.035f + randFromSeed(p.RandomSeed, 504.0f) * 0.025f,
            0.012f,
            randFromSeed(p.RandomSeed, 505.0f) * 6.2831853f,
            -2.0f + randFromSeed(p.RandomSeed, 506.0f) * 4.0f,
            0.48f + randFromSeed(p.RandomSeed, 507.0f) * 0.28f,
            1.0f,
            0.22f,
            0.015f,
            PARTICLE_FLAG_ADDITIVE | PARTICLE_FLAG_COLLIDE_GROUND | PARTICLE_FLAG_LOOPING,
            36u);
    }

    simulateParticle(p, float3(0.45f, 0.0f, 0.10f), 0.42f, 0.42f, 0.22f);
    ParticlesRW[i] = p;
}

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

    float lifeT = saturate(p.Age / max(p.LifeTime, 0.001f));
    float currentSize = lerp(p.StartSize, p.EndSize, lifeT);

    float2 offsets[6] =
    {
        float2(-1, -1), float2(-1, 1), float2(1, 1),
        float2(-1, -1), float2(1, 1), float2(1, -1)
    };

    float2 uvBase[6] =
    {
        float2(0, 1), float2(0, 0), float2(1, 0),
        float2(0, 1), float2(1, 0), float2(1, 1)
    };

    float s = sin(p.Rotation);
    float c = cos(p.Rotation);
    float2 rotated = float2(
        offsets[vId].x * c - offsets[vId].y * s,
        offsets[vId].x * s + offsets[vId].y * c);

    float3 velDir = normalize(p.Velocity + float3(0.0001f, 0.0001f, 0.0001f));
    float2 velBillboard = float2(dot(velDir, CamRight), dot(velDir, CamUp));
    float velLen = max(length(velBillboard), 0.0001f);
    float2 stretchAxis = velBillboard / velLen;
    float2 stretchPerp = float2(-stretchAxis.y, stretchAxis.x);
    float stretchAmount = saturate(length(p.Velocity) * p.Stretch * 0.045f);
    float along = dot(rotated, stretchAxis);
    float across = dot(rotated, stretchPerp);
    rotated = stretchAxis * along * (1.0f + stretchAmount) + stretchPerp * across;

    float3 worldPos = p.Position + (CamRight * rotated.x + CamUp * rotated.y) * currentSize;

    VSOut o;
    o.Pos = mul(float4(worldPos, 1.0f), ViewProj);

    float flicker = 0.92f + rand(p.Position.x * 2.7f + p.Position.y * 1.5f + p.Position.z * 2.1f + p.Age * 11.0f) * 0.18f;
    o.Color = lerp(p.StartColor, p.EndColor, lifeT) * flicker;

    uint frameCols = 6u;
    uint frameRows = max((p.FrameCount + frameCols - 1u) / frameCols, 1u);
    uint frameIndex = min(p.FrameIndex, p.FrameCount - 1u);
    uint col = frameIndex % frameCols;
    uint row = frameIndex / frameCols;

    float2 uv = uvBase[vId];
    uv.x = (uv.x + col) / (float)frameCols;
    uv.y = (uv.y + row) / (float)frameRows;

    o.UV = uv;
    return o;
}

Texture2D FireTex : register(t1);
SamplerState LinearSampler : register(s0);

float4 PS(VSOut i) : SV_Target
{
    float4 tex = FireTex.Sample(LinearSampler, i.UV);
    tex.rgb *= 1.12f;
    tex.a = saturate(pow(max(tex.a, 0.0f), 1.35f));
    return tex * i.Color;
}
