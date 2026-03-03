cbuffer cbBlurInfo : register(b0)
{
    float ScreenWidth;
    float ScreenHeight;
};

Texture2D<float4> gtxtInput0 : register(t0); // present renderTarget
//Texture2D gtxtInput1 : register(t1); // prev renderTarget
RWTexture2D<float4> BlurTexture : register(u0); // bluring result
SamplerState StaticSampler : register(s0);

[numthreads(8, 8, 1)]
void CSMain(int3 n3DispatchThreadID : SV_DispatchThreadID)
{
    if (n3DispatchThreadID.x >= ScreenWidth || n3DispatchThreadID.y >= ScreenHeight)
        return;

    int x = n3DispatchThreadID.x;
    int y = n3DispatchThreadID.y;
    float2 uv = float2((float) x / ScreenWidth, (float) y / ScreenHeight);
    float4 color0 = gtxtInput0.Load(int3(x, y, 0));
    float4 color1 = BlurTexture[int2(x, y)];
    BlurTexture[int2(x, y)] = 0.5f * (color0 + color1);
}