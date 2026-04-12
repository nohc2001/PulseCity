cbuffer cbBlurInfo : register(b0)
{
    float ScreenWidth;
    float ScreenHeight;
};

RWTexture2D<float4> gtxtInput0 : register(u0); // present renderTarget
Texture2D DepthTexture : register(t1); // DS SRV
RWTexture2D<float4> BlurTexture : register(u1); // bluring result
SamplerState StaticSampler : register(s0);

/*
12x12를 채우기 위해 64개의 스레드가 쓰이고 있다면
144개를 64개로 채워야 하기 때문에 한 스레드당 2개~3개의 픽셀을 불러올 수 있어야 한다.
*/
#define Margin 2;
groupshared float4 sharedColors[144];
groupshared float sharedDepth[144];
[numthreads(8, 8, 1)]
void CSMain(int3 n3DispatchThreadID : SV_DispatchThreadID)
{
    const float4 blank = float4(0, 0, 0, 0);
    int perX = 0;
    int perY = 0;
    int x = n3DispatchThreadID.x;
    int y = n3DispatchThreadID.y;
    float depth = DepthTexture.Load(int3(x, y, 0)).r;
    
    bool isin = n3DispatchThreadID.x < ScreenWidth && n3DispatchThreadID.y < ScreenHeight;
    if (isin)
    {
        int divX = x >> 3;
        int divY = y >> 3;
        perX = x & 7;
        perY = y & 7;
        uint index0 = 8 * perY + perX;
        int2 uv0 = int2(int(index0 % 12), int(index0 / 12));
        uint index1 = index0 + 64;
        int2 uv1 = int2(int(index1 % 12), int(index1 / 12));
        uint index2 = index1 + 64;
        int2 uv2 = int2(int(index2 % 12), int(index2 / 12));
    
        int2 ruv = int2(0, 0);
        float4 color0;
        float4 color1;
        float4 blend;
        int2 startUV = int2((divX * 8)-2, (divY * 8)-2);
    
        ruv = int2(startUV.x + uv0.x, startUV.y + uv0.y);
        if ((0 <= ruv.x && ruv.x < ScreenWidth) && (0 <= ruv.y && ruv.y < ScreenHeight))
        {
            color0 = gtxtInput0.Load(int3(ruv.x, ruv.y, 0));
            sharedColors[uv0.y * 12 + uv0.x] = color0;
            sharedDepth[uv0.y * 12 + uv0.x] = DepthTexture.Load(int3(ruv.x, ruv.y, 0));
        }
        else
        {
            sharedColors[uv0.y * 12 + uv0.x] = blank;
            sharedDepth[uv0.y * 12 + uv0.x] = 0.0f;
        }
    
        ruv = int2(startUV.x + uv1.x, startUV.y + uv1.y);
        if ((0 <= ruv.x && ruv.x < ScreenWidth) && (0 <= ruv.y && ruv.y < ScreenHeight))
        {
            color0 = gtxtInput0.Load(int3(ruv.x, ruv.y, 0));
            sharedColors[uv1.y * 12 + uv1.x] = color0;
            sharedDepth[uv1.y * 12 + uv1.x] = DepthTexture.Load(int3(ruv.x, ruv.y, 0));
        }
        else
        {
            sharedColors[uv1.y * 12 + uv1.x] = blank;
            sharedDepth[uv1.y * 12 + uv1.x] = 0.0f;
        }
    
        ruv = int2(startUV.x + uv2.x, startUV.y + uv2.y);
        if ((0 <= ruv.x && ruv.x < ScreenWidth) && (0 <= ruv.y && ruv.y < ScreenHeight))
        {
            color0 = gtxtInput0.Load(int3(ruv.x, ruv.y, 0));
            if (uv2.y * 12 + uv2.x < 144)
            {
                sharedColors[uv2.y * 12 + uv2.x] = color0;
                sharedDepth[uv2.y * 12 + uv2.x] = DepthTexture.Load(int3(ruv.x, ruv.y, 0));
            }
        }
        else
        {
            if (uv2.y * 12 + uv2.x < 144)
            {
                sharedColors[uv2.y * 12 + uv2.x] = blank;
                sharedDepth[uv2.y * 12 + uv2.x] = 0.0f;
            }
        }
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    if (isin)
    {
        const float weight[25] =
        {
            0.0030, 0.0133, 0.0219, 0.0133, 0.0030,
            0.0133, 0.0596, 0.0983, 0.0596, 0.0133,
            0.0219, 0.0983, 0.1621, 0.0983, 0.0219,
            0.0133, 0.0596, 0.0983, 0.0596, 0.0133,
            0.0030, 0.0133, 0.0219, 0.0133, 0.0030,
        };
        
        const float FarWeight[25] =
        {
            0.04, 0.04, 0.04, 0.04, 0.04,
            0.04, 0.04, 0.04, 0.04, 0.04,
            0.04, 0.04, 0.04, 0.04, 0.04,
            0.04, 0.04, 0.04, 0.04, 0.04,
            0.04, 0.04, 0.04, 0.04, 0.04,
        };
    
        float4 BlendingResult = blank;
        
        int2 xrange = int2(0, 5);
        xrange.x += int(depth * 2);
        xrange.y -= int(depth * 2);
        float mul = 0;
    //[unroll(5)]
        for (int iy = xrange.x; iy < xrange.y; ++iy)
        {
        //[unroll(5)]
            for (int ix = xrange.x; ix < xrange.y; ++ix)
            {
                int pindex = 12 * (perY + iy) + (perX + ix);
                float w = (depth * FarWeight[iy * 5 + ix] + (1.0f - depth) * weight[iy * 5 + ix]) * sharedDepth[pindex];
                mul += w;
                BlendingResult += w * sharedColors[pindex];
            }
        }
        float4 col = BlendingResult / mul;
        float4 blendcol = 0.5f * (col + BlurTexture[int2(x, y)]);
        BlurTexture[int2(x, y)] = blendcol;
        gtxtInput0[int2(x, y)] = blendcol;
    }
}