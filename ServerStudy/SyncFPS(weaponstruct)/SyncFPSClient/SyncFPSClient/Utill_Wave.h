#pragma once
#include <stdio.h>
#include <iostream>
#include <conio.h>
#include <chrono>
#include <windows.h>
#include "stdafx.h"
#pragma comment(lib, "winmm.lib")

//fl studio 의 구조를 복사한 것.
// 1. 더 나은 구조 있는지 탐색
// 2. 패처를 만들고 기능의 확장.
// 3. Effect와 Controller를 통일

/*
* 1. VZEROUPPER 로 AVX 사용.
* 2. 체널 통과나 믹싱 중에는 short 대신 float 사용.
    왜? 벡터 연산이 더 빠름.
    대신 내보낼때는 short로 내보내야 함.
    때문에 마스터 채널만 short이고, 나머지 체널은 다 float여야 함.

이펙터도 만드셈..
*/

// 9 : 100hz, 0.01 seconds
// 10 : 50hz, 0.02 seconds
// 11 : 25hz, 0.04 seconds clock : 132 000 000
// 12 : 12.5hz, 0.08 seconds
// 13 : 6.25hz, 0.16 seconds
constexpr ui32 updateSampleCount = 1 << 11;
constexpr ui32 updateSampleCount_pow2 = 11;


constexpr float Ra[2] = { 2, 0 };
constexpr float Rb[2] = { 0, 1 };
constexpr float La[2] = { 0, -2 };
constexpr float Lb[2] = { 1, 2 };

struct float32 {
    union {
        float f[32];
        __m256 f8[4] = {};
    };
};

struct float16 {
    union {
        float f[16];
        __m256 f8[2] = {};
    };
};

struct float8 {
    union {
        float f[8] = {};
        __m256 f8;
    };
};

struct float4 {
    union {
        float f[4] = {};
        __m128 f4;
    };
};

struct float2 {
    float f[2];
};

//asm procedure
extern "C" {
    void __stdcall sin01_32(float32* f32);
    void __stdcall inharmonic8_sin01_4samples(float8* A, float8* phaze, float8* x);
    void __stdcall inharmonic16_sin01_2samples(float16* A, float16* phaze, float2* x);
    void __stdcall inharmonic32_sin01_1samples(float32* A, float32* phaze, float* x);
}

//basic pulse x= 0 ~ 1; y= -1 ~ 1;
#pragma optimize( "x", on )
float sin01(float x) {
    //##0 ready for execute _ define constexpr values
    constexpr float A[4] = { 0.25f, -0.25f, 0.25f, -0.25f };
    constexpr float B[4] = { 0, 0.5f, -0.5f, 1.0f };//function f(x) = /\/\ domain [0~4]; y = Ax+B
    constexpr float R[4] = { 1, 1, -1, -1 };//function g(x) = __-- domain [0~4]; y = R;
    constexpr float n3 = 0.16666666f; // 1/3!
    constexpr float n5 = 0.008f; // 1/5!
    constexpr float n7 = 0.00019047619f; // 1/7!
    constexpr float n9 = 0.0000027557f; // 1/9! for tayler approximate
    //##1
    x -= (int)x; // x = x - floor(x)  |   result : {0 <= x <= 1}
    //##2
    x = 4.0f * x; // {0 <= x <= 1} -> {0 <= x <= 4}
    //##3
    ui32 index = (int)x; // selector index of A, B, R
    index &= 3;
    //##4
    x = A[index] * x;
    //##5
    x += B[index]; // x = f(x);
    //##6
    x = 6.28318530718f * x; // x = 2 * pi * x; ---- because sin(2*pi*x) repeat in {x = 0 ~ 1}
    float preresult = x;
    //##7
    float dx = x * x;
    //##8
    float fx = dx * dx;
    float x3 = dx * x;
    //##9
    float x5 = fx * x;
    float x7 = fx * x3;
    preresult -= x3 * n3;
    //##10
    float x9 = x5 * fx;
    float temp0 = x5 * n5;
    float temp1 = x7 * n7;
    //##11
    preresult += temp0;
    float temp2 = x9 * n9;
    //##12
    preresult -= temp1;
    //##13
    preresult += temp2;
    //##14
    return R[index] * preresult;

    // 14 step func, 1.35(19/14)work per 1 step.
}
#pragma optimize( "x", off )

float tri01(float x) {
    constexpr float A[4] = { 4, -4, -4, 4 };
    constexpr float B[4] = { 0, 2, 2, -4 };
    x -= (int)x;
    float cx = 4.0f * x; // {0 <= x <= 1} -> {0 <= x <= 4}
    ui32 index = (int)cx; // selector index of A, B, R
    index &= 3;
    return A[index] * x + B[index];
}

float squ01(float x) {
    constexpr float A[2] = { 1, -1 };
    x -= (int)x;
    ui32 index = (int)(4.0f * x);
    index &= 3;
    return A[index];
}

float saw01(float x) {
    x -= (int)x;
    return -2 * x + 1;
}
//
//float sin_harmonic16_01(float x) {
//    //##0 ready for execute _ define constexpr values
//    constexpr float A[4] = { 0.25f, -0.25f, 0.25f, -0.25f };
//    constexpr float B[4] = { 0, 0.5f, -0.5f, 1.0f };//function f(x) = /\/\ domain [0~4]; y = Ax+B
//    constexpr float R[4] = { 1, 1, -1, -1 };//function g(x) = __-- domain [0~4]; y = R;
//    constexpr float n3 = 0.16666666f; // 1/3!
//    constexpr float n5 = 0.008f; // 1/5!
//    constexpr float n7 = 0.00019047619f; // 1/7!
//    constexpr float n9 = 0.0000027557f; // 1/9! for tayler approximate
//    float hx[16] = {};
//    //##1
//    x -= (int)x; // x = x - floor(x)  |   result : {0 <= x <= 1}
//    //##2
//    x = 4.0f * x; // {0 <= x <= 1} -> {0 <= x <= 4}
//
//    hx[0] = x;
//    hx[1] = x * 2.0f;
//    hx[2] = x * 3.0f;
//    hx[3] = x * 4.0f;
//    hx[4] = x * 5.0f;
//    hx[5] = x * 6.0f;
//    hx[6] = x * 7.0f;
//    hx[7] = x * 8.0f;
//    hx[8] = x * 9.0f;
//    hx[9] = x * 10.0f;
//    hx[10] = x * 11.0f;
//    hx[11] = x * 12.0f;
//    hx[12] = x * 13.0f;
//    hx[13] = x * 14.0f;
//    hx[14] = x * 15.0f;
//    hx[15] = x * 16.0f;
//
//    //##3
//    ui32 index[16] = {}; // selector index of A, B, R
//    index[0] = (int)hx[0]; 
//    index[1] = (int)hx[1];
//    index[2] = (int)hx[2];
//    index[3] = (int)hx[3];
//    index[4] = (int)hx[4];
//    index[5] = (int)hx[5];
//    index[6] = (int)hx[6];
//    index[7] = (int)hx[7];
//    index[8] = (int)hx[8];
//    index[9] = (int)hx[9];
//    index[10] = (int)hx[10];
//    index[11] = (int)hx[11];
//    index[12] = (int)hx[12];
//    index[13] = (int)hx[13];
//    index[14] = (int)hx[14];
//    index[15] = (int)hx[15];
//
//    index[0] &= 3;
//    index[1] &= 3;
//    index[2] &= 3;
//    index[3] &= 3;
//    index[4] &= 3;
//    index[5] &= 3;
//    index[6] &= 3;
//    index[7] &= 3;
//    index[8] &= 3;
//    index[9] &= 3;
//    index[10] &= 3;
//    index[11] &= 3;
//    index[12] &= 3;
//    index[13] &= 3;
//    index[14] &= 3;
//    index[15] &= 3;
//
//    //##4
//    hx[0] *= A[index[0]];
//    //##5
//    x += B[index]; // x = f(x);
//    //##6
//    x = 6.28318530718f * x; // x = 2 * pi * x; ---- because sin(2*pi*x) repeat in {x = 0 ~ 1}
//    float preresult = x;
//    //##7
//    float dx = x * x;
//    //##8
//    float fx = dx * dx;
//    float x3 = dx * x;
//    //##9
//    float x5 = fx * x;
//    float x7 = fx * x3;
//    preresult -= x3 * n3;
//    //##10
//    float x9 = x5 * fx;
//    float temp0 = x5 * n5;
//    float temp1 = x7 * n7;
//    //##11
//    preresult += temp0;
//    float temp2 = x9 * n9;
//    //##12
//    preresult -= temp1;
//    //##13
//    preresult += temp2;
//    //##14
//    return R[index] * preresult;
//
//    // 14 step func, 1.35(19/14)work per 1 step.
//}

float y_equal_x(float x) {
    return x;
}

float cos_float(float x) {
    return 0.5f - 0.5f * cosf(3.141592f * x);
}

__forceinline float GetPanL(float pan) {
    ui32 PanGraphindex = (int)(pan / 0.5f);
    return La[PanGraphindex] * pan + Lb[PanGraphindex];
}

__forceinline float GetPanR(float pan) {
    ui32 PanGraphindex = (int)(pan / 0.5f);
    return Ra[PanGraphindex] * pan + Rb[PanGraphindex];
}

struct AirPressureLR {
    si16 L;
    si16 R;
}; // 4byte

struct WaveDataStruct {
    AirPressureLR* SampleBuffer = nullptr;
    ui32 bufferSize = 0; // buffer size 는 무조건 2의 거듭제곱 형태 -> %대신 &로 하기 위해

    WaveDataStruct() {};

    void Alloc(ui32 buffersiz_pow2) {
        bufferSize = 1 << buffersiz_pow2;
        SampleBuffer = new AirPressureLR[bufferSize];
    }

    void Release() {
        delete[] SampleBuffer;
    }
};

struct WaveParameter {
    string name;
    float data;

    void Init(const char* param_name, float init_data) {
        name = param_name;
        data = init_data;
    }
};

struct PRD_Note {
    float startTime;
    float endTime;
    float frequency;
};

struct WaveScale {
    si32 frequncyCount;
    ui8 mantissa;
    vector<float> table;
    static constexpr float StandardFrequency = 60;
    static constexpr float minFrequency = 20;
    static constexpr float maxFrequency = 22500;

    void Init(ui32 fc = 21, ui32 mantissa = 3) {
        frequncyCount = fc;
        mantissa = 3;
        float startF = StandardFrequency;
        float sf = startF;
        if (mantissa <= 3) {
            startF /= mantissa;
        }
        int totalcount = 0;

        while (true) {
            if (startF * (float)mantissa < maxFrequency) {
                totalcount += frequncyCount;
                startF = startF * (float)mantissa;
                continue;
            }
            else {
                float f = 0;
                for (int i = 0; i < frequncyCount; ++i) {
                    totalcount += 1;
                    f = startF * powf(mantissa, ((float)(i)) / (float)(frequncyCount));
                    if (f >= maxFrequency) {
                        goto CALCUL_TOTALF;
                    }
                }
            }
        }

    CALCUL_TOTALF:
        table.reserve(totalcount);
        for (int i = 0; i < totalcount; ++i) {
            table.push_back(sf * powf((float)mantissa, (float)(i) / (float)(frequncyCount)));
        }
    }

    float GetF(ui32 height, ui32 id) {
        return table[height * frequncyCount + id];
    }

    void Release() {
        table.clear();
    }
};

//Piano Roll Data
struct PRD {
    vector<PRD_Note> notes;
    WaveScale scale;
};

struct Graph2D {
    //fmCombinedGraph2d<float, float> data;
};

struct WaveGraph2D {
    //fmCombinedGraph2d<ui8, short> data;
};

// 아직 그래프를 적응 못시킴.
struct WaveEnvelop {
    float Attack;
    float Decay;
    float DecayVolumeRate;
    float Sustain;
    float Release;
    //fmContinuousGraph_OuterFunctionMode<float, float> graph;
    WaveParameter* controlparam = nullptr;
    // Attack + Decay + Sustain -> Release Time Point.

    void Init(float attack, float decay, float decayVol, float sustain, float release) {
        Attack = attack;
        Decay = decay;
        DecayVolumeRate = decayVol;
        Sustain = sustain;
        Release = release;
    }

    void Compiled(float (*outfunc)(float), WaveParameter* param) {
        // outerfunction continuous graph
        // 0~attack : 0 ~ 1
        // attack~Decay : 1 ~ DecayVolumeRate
        // Decay ~ Sustain : DecayVolumeRate
        // Sustain ~ Release : DecayVolumeRate ~ 0

       /* fmvecarr<pos2f<float, float>> poses;
        poses.NULLState();
        poses.Init(5);
        poses.push_back(pos2f<float, float>(0, 0));
        poses.push_back(pos2f<float, float>(Attack, 1));
        poses.push_back(pos2f<float, float>(Attack + Decay, DecayVolumeRate));
        poses.push_back(pos2f<float, float>(Attack + Decay + Sustain, DecayVolumeRate));
        poses.push_back(pos2f<float, float>(Attack + Decay + Sustain + Release, 0));
        graph.Compile_Graph(outfunc, poses);
        graph.graphdata.clean();*/

        controlparam = param;
    }

    __forceinline float operator[](float time) {
        return time;//graph[time];
    }

    __forceinline float GetReleaseTimePoint() {
        return Attack + Decay + Sustain;
    }
};

struct WaveEffector {
    string name;
    vector<WaveParameter> parameters;
    AirPressureLR* (*func)(WaveEffector*, WaveDataStruct*, ui32, ui32) = nullptr;
    // parameter : effector, wavestrut, startpivot, sampleLength
};

struct WaveEffect {
    WaveEffector* origin;
    vector<float> paramdata;

    void Release() {
        origin = 0;
        paramdata.clear();
    }
};

struct WaveAutomation_Instance {
    Graph2D* origin_automation;
    WaveEffect* fx;
    float startTime;
    float startCut, endCut;
    ui16 repeatNum;
};

struct MasterChannel {
    HWAVEOUT hWaveDev;
    WAVEFORMATEX wf;
    WAVEHDR hdr;
    ui32 pivot;
    WaveDataStruct sound;
    bool exit_signal = false;
    HANDLE threadHandle = 0;
    HANDLE channelThread = 0;

    void Init(ui32 masterbuff_sampleLength_pow2);

    void StartPlaying_MasterChannel();

    void Streaming_MasterChannel(ui32 headerCount);

    void ReleaseMaster() {
        exit_signal = true;

        while (exit_signal) {}
        CloseHandle(threadHandle);
    }
};

MasterChannel wave_master_channel;

DWORD WINAPI WaveMasterThread(LPVOID prc) {
    HANDLE hMainThread = GetCurrentThread();

    if (hMainThread == NULL) {
        printf("Failed to get the main thread handle\n");
        return 1;
    }

    DWORD_PTR dwThreadAffinityMask = 1; // 예: Core 0
    if (!SetThreadAffinityMask(hMainThread, dwThreadAffinityMask)) {
        printf("SetThreadAffinityMask failed\n");
        return 1;
    }

    // Time Critical은 위험하지 않을까? ..
    if (!SetThreadPriority(hMainThread, THREAD_PRIORITY_HIGHEST)) {
        printf("SetThreadPriority failed\n");
        return 1;
    }

    int priority = GetThreadPriority(hMainThread);
    if (priority == THREAD_PRIORITY_ERROR_RETURN) {
        printf("GetThreadPriority failed\n");
        return 1;
    }
    printf("Thread priority is set to %d\n", priority);

    // yeild and revisit -> update priority and affinity.
    Sleep(100);

    //wave_master_channel.Init(23); // 3minute
    wave_master_channel.Streaming_MasterChannel(4);
    return 0;
}

struct WaveChannel {
    bool is_flowing = false;
    WaveDataStruct sound;
    unsigned int samplePlayPivot = 0;
    float Volume;
    float Pan;
    vector<WaveEffect> effectContainer;
    vector<WaveChannel*> last_output_goto;
    bool sendToMaster = false;

    void Init(ui32 pow2, bool send_to_master) {
        sound.Alloc(pow2);
        samplePlayPivot = 0;
        Volume = 1;
        Pan = 0.5f;
        effectContainer.reserve(8);
        last_output_goto.reserve(8);
        sendToMaster = send_to_master;
        is_flowing = true;
    }

    // when static sound send to channel
    void pushWave(WaveDataStruct wave) {
        ui32 Cper = sound.bufferSize - 1;
        ui32 s = samplePlayPivot + updateSampleCount;
        for (int i = 0; i < wave.bufferSize; ++i) {
            sound.SampleBuffer[(s + i) & Cper] = wave.SampleBuffer[i];
        }
    }
    // when static sound send and combine to channel
    void pushAddWave(WaveDataStruct wave) {
        ui32 Cper = sound.bufferSize - 1;
        for (int i = 0; i < wave.bufferSize - 16; i += 16) {
            int si = (samplePlayPivot + i) & Cper;
            *(__m128i*)& sound.SampleBuffer[si] = _mm_adds_epi16(*(__m128i*) & sound.SampleBuffer[si], *(__m128i*) & wave.SampleBuffer[i]);
            *(__m128i*)& sound.SampleBuffer[si + 4] = _mm_adds_epi16(*(__m128i*) & sound.SampleBuffer[si + 4], *(__m128i*) & wave.SampleBuffer[i + 4]);
            *(__m128i*)& sound.SampleBuffer[si + 8] = _mm_adds_epi16(*(__m128i*) & sound.SampleBuffer[si + 8], *(__m128i*) & wave.SampleBuffer[i + 8]);
            *(__m128i*)& sound.SampleBuffer[si + 12] = _mm_adds_epi16(*(__m128i*) & sound.SampleBuffer[si + 12], *(__m128i*) & wave.SampleBuffer[i + 12]);
            
            //_mm512_adds_epi16()
            //sound.SampleBuffer[si].L += wave.SampleBuffer[i].L;
            //sound.SampleBuffer[si].R += wave.SampleBuffer[i].R;
        }
    }

    void ApplyEffects() {
        ui32 apply_sampleLen = updateSampleCount;
        for (int i = 0; i < effectContainer.size(); ++i) {
            effectContainer.at(i).origin->func(effectContainer.at(i).origin, &sound, samplePlayPivot, apply_sampleLen);
        }

        ui32 Cper = sound.bufferSize - 1;
        for (int i = samplePlayPivot; i < samplePlayPivot + apply_sampleLen; ++i) {
            ui32 si = i & Cper;
            sound.SampleBuffer[si].L = sound.SampleBuffer[si].L * Volume * GetPanL(Pan);
            sound.SampleBuffer[si].R = sound.SampleBuffer[si].R * Volume * GetPanR(Pan);
        }
    }

    void SendToChannels() {
        ui32 apply_sampleLen = updateSampleCount;
        for (int i = 0; i < last_output_goto.size(); ++i) {
            WaveDataStruct& w = last_output_goto.at(i)->sound;
            ui32 Cper = w.bufferSize - 1;
            ui32 ocper = sound.bufferSize - 1;
            ui32 startpivot = last_output_goto.at(i)->samplePlayPivot;
            for (int i = 0; i < apply_sampleLen; ++i) {
                ui32 si = (startpivot + i) & Cper;
                ui32 oi = (samplePlayPivot + i) & ocper;
                w.SampleBuffer[si].L += sound.SampleBuffer[oi].L;
                w.SampleBuffer[si].R += sound.SampleBuffer[oi].R;
            }
        }

        if (sendToMaster) {
            WaveDataStruct& w = wave_master_channel.sound;
            ui32 Cper = w.bufferSize - 1;
            ui32 ocper = sound.bufferSize - 1;

            ui32 startpivot = wave_master_channel.pivot >> updateSampleCount_pow2;
            startpivot += 2;
            startpivot <<= updateSampleCount_pow2;

            for (int i = 0; i < apply_sampleLen; ++i) {
                ui32 si = (startpivot + i) & Cper;
                ui32 oi = (samplePlayPivot + i) & ocper;
                w.SampleBuffer[si].L += sound.SampleBuffer[oi].L;
                w.SampleBuffer[si].R += sound.SampleBuffer[oi].R;
            }
        }
    }

    void ChannelStep() {
        ApplyEffects();
        SendToChannels();

        ui32 ocper = sound.bufferSize - 1;
        ui32 gap = sound.bufferSize >> 1;
        for (int i = samplePlayPivot + gap; i < samplePlayPivot + gap + updateSampleCount; ++i) {
            sound.SampleBuffer[i & ocper].L = 0;
            sound.SampleBuffer[i & ocper].R = 0;
        }
        samplePlayPivot += updateSampleCount;
        samplePlayPivot &= (sound.bufferSize - 1);
    }

    void Release() {
        sound.Release();
        for (int i = 0; i < effectContainer.size(); ++i) {
            effectContainer[i].Release();
        }
        last_output_goto.clear();
    }
};

struct WaveInstrument {
    vector<WaveGraph2D*> BasicWaveTable;
    vector<WaveEnvelop> envelops;
    ui32 minimumOutputSampleLength; // release total time
    WaveParameter volume;
    WaveParameter pan;

    void(*outFunc)(WaveDataStruct, float /*frequncy;*/) = nullptr;
    //WaveChannel* send_channel;

    void Init() {
        BasicWaveTable.reserve(8);
        envelops.reserve(8);
        WaveEnvelop envelop;
        envelop.Init(1.0f, 0.1f, 0.2f, 3.0f, 1.0f);
        envelop.Compiled(cos_float, nullptr);
        envelops.push_back(envelop);
        minimumOutputSampleLength = (ui32)(44100.0f * envelop.Release);
        outFunc = nullptr;
        volume.Init("volume", 1.0f);
        pan.Init("pan", 0.5f);
    }

    WaveDataStruct Result(float frequncy, float inputTime) {
        constexpr ui32 second_interval = 44100;
        static AirPressureLR* tempBuffer = new AirPressureLR[second_interval * 30];
        WaveEnvelop volume_env = envelops.at(0);
        WaveDataStruct r;
        float attackTimePoint = volume_env.Attack + volume_env.Decay;
        ui32 holdtime = (int)((float)second_interval * inputTime);
        if (inputTime - attackTimePoint > 0) {
            holdtime = (int)((float)second_interval * (inputTime - attackTimePoint));
        }
        else {
            holdtime = attackTimePoint;
        }
        float releaseTimePoint = volume_env.GetReleaseTimePoint();
        ui32 releaseSampleCount = (ui32)(volume_env.Release * second_interval);
        r.bufferSize = holdtime + releaseSampleCount;
        r.SampleBuffer = tempBuffer;
        if (outFunc != nullptr) {
            outFunc(r, frequncy);
        }
        else {
            //initial instrument. (sin wave with envelop)

            //attack - decay
            for (int i = 0; i < attackTimePoint; ++i) {
                float _time = (float)i / (float)second_interval;
                float value = volume_env[_time] * sin01(frequncy * _time);
                r.SampleBuffer[i].L = 32767.0f * value * GetPanL(pan.data);
                r.SampleBuffer[i].R = 32767.0f * value * GetPanR(pan.data);
            }

            //hold
            for (int i = attackTimePoint; i < holdtime; ++i) {
                float _time = (float)i / (float)second_interval;
                float value = volume_env[_time] * sin01(frequncy * _time);
                r.SampleBuffer[i].L = 32767.0f * value * GetPanL(pan.data);
                r.SampleBuffer[i].R = 32767.0f * value * GetPanR(pan.data);
            }

            //release
            if (releaseTimePoint >= inputTime) {
                for (int i = holdtime; i < r.bufferSize; ++i) {
                    float _time = (float)i / (float)second_interval;
                    float _rtime = (float)(i - holdtime) / (float)second_interval;
                    float value = volume_env[_rtime + releaseTimePoint] * sin01(frequncy * _time);
                    r.SampleBuffer[i].L = 32767.0f * value * GetPanL(pan.data);
                    r.SampleBuffer[i].R = 32767.0f * value * GetPanR(pan.data);
                }
            }
        }
        return r;
    }
};

struct PRD_Instance {
    PRD* origin;
    WaveInstrument* instrument;
    float startTime;
    float startCut, endCut;
    unsigned short repeatNum;
};

vector<WaveChannel> channelGroup;

DWORD WINAPI WaveChannelThread(LPVOID prc) {
    ui32 ft = 0;
    ui32 et = 0;
    while (true) {
        et = wave_master_channel.pivot;
        while (et - ft < updateSampleCount) {
            et = wave_master_channel.pivot;
        }
        ft = et;
        for (int i = 0; i < channelGroup.size(); ++i) {
            channelGroup[i].ChannelStep();// 43.xxxHz
        }
    }
    return 0;
}

void MasterChannel::Init(ui32 masterbuff_sampleLength_pow2) {
    exit_signal = false;

    hdr = { NULL, };
    hdr.dwLoops = -1; // 무한 루프?

    wf.cbSize = sizeof(WAVEFORMATEX);
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = 2;
    wf.wBitsPerSample = 16;
    wf.nSamplesPerSec = 44100;
    wf.nBlockAlign = wf.nChannels * wf.wBitsPerSample / 8;
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
    waveOutOpen(&hWaveDev, WAVE_MAPPER, &wf, (DWORD)NULL, 0, CALLBACK_NULL);

    ui32 samplelen = pow(2, masterbuff_sampleLength_pow2);
    sound.bufferSize = samplelen;
    sound.SampleBuffer = new AirPressureLR[sound.bufferSize];
    ZeroMemory(sound.SampleBuffer, sound.bufferSize * sizeof(AirPressureLR));

    hdr.lpData = (char*)sound.SampleBuffer;
    hdr.dwBufferLength = sound.bufferSize * sizeof(AirPressureLR);

    channelGroup.reserve(32);

    threadHandle = CreateThread(NULL, 0, WaveMasterThread, 0, 0, NULL);
    //channelThread = CreateThread(NULL, 0, WaveChannelThread, 0, 0, NULL);
}

void MasterChannel::StartPlaying_MasterChannel() {
    const si32 gap = sound.bufferSize / 2;
    const ui32 per = sound.bufferSize - 1;

    si32 fs = 0;
    si32 es = 0;

    while (true) {
        // 준비 및 출력한다.
        waveOutPrepareHeader(hWaveDev, &hdr, sizeof(WAVEHDR));
        waveOutWrite(hWaveDev, &hdr, sizeof(WAVEHDR));

        // 다 재생할 때까지 대기한다.
        si32 fpivot = 0;
        while (waveOutUnprepareHeader(hWaveDev, &hdr, sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) {
            //waveOutGetPosition(hWaveDev, &pivot, sizeof(MMTIME));

            es = wave_master_channel.pivot;
            if (es - fs > updateSampleCount) {
                for (int i = 0; i < channelGroup.size(); ++i) {
                    channelGroup[i].ChannelStep();// 43.xxxHz
                }
                fs += updateSampleCount;
            }

            if (exit_signal) {
                waveOutUnprepareHeader(hWaveDev, &hdr, sizeof(WAVEHDR));
                goto WaveMasterChannel_Playing_END;
            }

            for (int i = fpivot - gap; i < (si32)pivot - gap; ++i) {
                sound.SampleBuffer[i & per].L = 0;
                sound.SampleBuffer[i & per].R = 0;
            }
            fpivot = pivot;
        }
        for (int i = fpivot; i < sound.bufferSize; ++i) {
            sound.SampleBuffer[i].L = 0;
            sound.SampleBuffer[i].R = 0;
        }
        // 뒷정리한다.
        waveOutUnprepareHeader(hWaveDev, &hdr, sizeof(WAVEHDR));
        cout << "master loop" << endl;
    }

WaveMasterChannel_Playing_END:
    //free(samplebuf);
    for (int i = 0; i < channelGroup.size(); ++i) {
        channelGroup[i].Release();// 43.xxxHz
    }

    delete[] sound.SampleBuffer;

    waveOutClose(hWaveDev);

    exit_signal = false;
}

void MasterChannel::Streaming_MasterChannel(ui32 headerCount) {
    const si32 gap = sound.bufferSize / 2;
    const ui32 per = sound.bufferSize - 1;

    constexpr ui32 seclen = updateSampleCount * 4;
    constexpr ui32 update_sample_count = updateSampleCount;
    WAVEHDR hdr[128] = {};

START_STREAM:

    for (int i = 0; i < headerCount; ++i) {
        hdr[i].dwLoops = -1;
    }

    // 헤더에 버퍼와 길이를 지정한다.
    hdr[0].lpData = (char*)sound.SampleBuffer;
    hdr[0].dwBufferLength = seclen;

    for (int i = 1; i < headerCount; ++i) {
        hdr[i].lpData = hdr[0].lpData - (headerCount - i) * seclen;
        hdr[i].dwBufferLength = seclen;
    }

    ui32 niarr[128] = {};
    for (int i = 0; i < headerCount; ++i) {
        niarr[i] = (i + 1) % headerCount;
    }

    ui64 ft = GetTicks();
    ui64 et = GetTicks();
    pivot = (ui32)0 - updateSampleCount;
    ui32 deltaTime = (ui32)((float)updateSampleCount * (float)QUERYPERFORMANCE_HZ / 44100.0f);
    char* limitptr = ((char*)sound.SampleBuffer) + 4 * sound.bufferSize;
    while (true) {
        // 준비 및 출력한다.
        if (exit_signal) goto WAVEMASTERCHANNEL_STREAMING_EXIST;

        for (int i = 0; i < headerCount; ++i) {
            int ni = niarr[i];

            ui64 cft = GetTicks();
            for (int i = 0; i < channelGroup.size(); ++i) {
                channelGroup[i].ChannelStep();// 43.xxxHz
            }
            ui64 cet = GetTicks();

            for (ui32 i = pivot + gap - updateSampleCount; i < pivot + gap; ++i) {
                sound.SampleBuffer[i & per].L = 0;
                sound.SampleBuffer[i & per].R = 0;
            }

            waveOutPrepareHeader(hWaveDev, &hdr[i], sizeof(WAVEHDR));
            et = GetTicks();
            while (et - ft < deltaTime) {
                et = GetTicks();
            }//
            ft = GetTicks();//
            waveOutWrite(hWaveDev, &hdr[i], sizeof(WAVEHDR));//
            pivot = (pivot + updateSampleCount) & per;//
            while (waveOutUnprepareHeader(hWaveDev, &hdr[ni], sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) {
            }
            waveOutUnprepareHeader(hWaveDev, &hdr[ni], sizeof(WAVEHDR));
            if (hdr[ni].lpData + (headerCount + 1) * seclen < ((char*)sound.SampleBuffer) + 4 * sound.bufferSize) {
                hdr[ni].lpData += headerCount * seclen;
            }
            else {
                goto WAVEMASTERCHANNEL_STREAMING_END;
            }
        }
    }

WAVEMASTERCHANNEL_STREAMING_END:

    for (int i = 0; i < headerCount; ++i) {
        while (waveOutUnprepareHeader(hWaveDev, &hdr[i], sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) {
        }
    }
    for (int i = 0; i < headerCount; ++i) {
        waveOutUnprepareHeader(hWaveDev, &hdr[i], sizeof(WAVEHDR));
    }

    goto START_STREAM;

WAVEMASTERCHANNEL_STREAMING_EXIST:
    for (int i = 0; i < headerCount; ++i) {
        while (waveOutUnprepareHeader(hWaveDev, &hdr[i], sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) {
        }
    }
    for (int i = 0; i < headerCount; ++i) {
        waveOutUnprepareHeader(hWaveDev, &hdr[i], sizeof(WAVEHDR));
    }

    waveOutClose(hWaveDev);

}

WaveChannel* NewChannel() {
    WaveChannel channel0;
    channel0.Init(23, true);
    channelGroup.push_back(channel0);
    return &channelGroup[channelGroup.size()-1];
}

// WaveOut을 한 후에도 버퍼를 바꾸면 반영이 된다.
// 때문에 우리는 그냥 사운드를 틀어놓고, 추가하고자 하는 사운드가 있다면 피벗의 뒤에 사운드를 입력해주면 된다.
// 실시간 이펙트는 그냥 셈플버퍼에 이펙트를 적용시키면 된다. (이펙트를 코드로 만들어서)
// 만약 게속 써야하는 배경음악 같은 사운드가 있다면, 원본 데이터를 따로 두고, 메인 버퍼에 게속해서 써 넣는 형식이면 된다.
// 이 버퍼들을 채널이라 생각하고 만들면 될것 같다.

//waveOutRestart(hWaveDev);
//waveOutPause(hWaveDev);
//waveOutGetVolume(hWaveDev, &vol);
//waveOutSetVolume(hWaveDev, vol);
//waveOutGetPosition
//waveOutGetPosition
//waveOutGetPitch
//waveOutSetPitch

typedef unsigned long long ui64;

void ShowSoundDevices()
{
    UINT wavenum;
    char devname[128];
    wavenum = waveOutGetNumDevs();
    printf("장치 개수 = %d\n", wavenum);
    WAVEOUTCAPS cap;
    for (UINT i = 0; i < wavenum; i++) {
        waveOutGetDevCaps(i, &cap, sizeof(WAVEOUTCAPS));
        WideCharToMultiByte(CP_ACP, 0, cap.szPname, -1, devname, 128, NULL, NULL);
        printf("%d번 : %d 채널,지원 포맷=%x,기능=%x,이름=%s\n",
            i, cap.wChannels, cap.dwFormats, cap.dwSupport, devname);
    }
}

WaveDataStruct CreateWaveFromFile(const wchar_t* filename) {
    WaveDataStruct r;
    r.bufferSize = 0;
    r.SampleBuffer = nullptr;

    HWAVEOUT hWaveDev;
    HANDLE hFile;
    DWORD filesize;
    DWORD dwRead;
    char* samplebuf;
    WAVEFORMATEX wf;

    // 웨이브 파일을 연다.
    hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        puts("file not found");
        return r;
    }

    // 재생 장치를 연다.
    wf.cbSize = sizeof(WAVEFORMATEX);
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = 2;
    wf.wBitsPerSample = 16;
    wf.nSamplesPerSec = 44100;
    wf.nBlockAlign = wf.nChannels * wf.wBitsPerSample / 8;
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
    waveOutOpen(&hWaveDev, WAVE_MAPPER, &wf, (DWORD)NULL, 0, CALLBACK_NULL);

    // 헤더는 건너 뛰고 버퍼에 샘플 데이터를 읽어들인다.
    SetFilePointer(hFile, 44, NULL, SEEK_SET);
    filesize = GetFileSize(hFile, NULL) - 44;
    samplebuf = (char*)malloc(filesize);
    BOOL b = ReadFile(hFile, samplebuf, filesize, &dwRead, NULL);
    if (b) {
        r.bufferSize = filesize >> 2; // number of sample (airpressure)
        r.SampleBuffer = (AirPressureLR*)samplebuf;
    }
    return r;
}

int PlaySoundFile(const wchar_t* filename)
{
    HWAVEOUT hWaveDev;
    HANDLE hFile;
    DWORD filesize;
    DWORD dwRead;
    char* samplebuf;
    WAVEFORMATEX wf;
    WAVEHDR hdr = { NULL, };
    hdr.dwLoops = -1; // 무한 루프?

    // 웨이브 파일을 연다.
    hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        puts("file not found");
        return -1;
    }

    // 재생 장치를 연다.
    wf.cbSize = sizeof(WAVEFORMATEX);
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = 2;
    wf.wBitsPerSample = 16;
    wf.nSamplesPerSec = 44100;
    wf.nBlockAlign = wf.nChannels * wf.wBitsPerSample / 8;
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
    waveOutOpen(&hWaveDev, WAVE_MAPPER, &wf, (DWORD)NULL, 0, CALLBACK_NULL);

    // 헤더는 건너 뛰고 버퍼에 샘플 데이터를 읽어들인다.
    SetFilePointer(hFile, 44, NULL, SEEK_SET);
    filesize = GetFileSize(hFile, NULL) - 44;
    samplebuf = (char*)malloc(filesize);
    ReadFile(hFile, samplebuf, filesize, &dwRead, NULL);

    // 헤더에 버퍼와 길이를 지정한다.
    hdr.lpData = samplebuf;
    hdr.dwBufferLength = filesize;

    // 준비 및 출력한다.
    waveOutPrepareHeader(hWaveDev, &hdr, sizeof(WAVEHDR));
    waveOutWrite(hWaveDev, &hdr, sizeof(WAVEHDR));

    cout << "PlayStart" << endl;

    // 다 재생할 때까지 대기한다.
    while (waveOutUnprepareHeader(hWaveDev, &hdr, sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) {
        Sleep(100);
        MMTIME pivot;
        waveOutGetPosition(hWaveDev, &pivot, sizeof(MMTIME));
        cout << pivot.u.sample << endl;//sample = 1/44100 second. in mono sound 
    }
    // 뒷정리한다.
    waveOutUnprepareHeader(hWaveDev, &hdr, sizeof(WAVEHDR));

    free(samplebuf);

    waveOutClose(hWaveDev);

    CloseHandle(hFile);

    return 0;
}

int PlaySoundFileStream(const wchar_t* filename)
{
    static ui64 et, ft;
    HWAVEOUT hWaveDev;
    HANDLE hFile;
    DWORD dwRead;
    char* samplebuf;
    DWORD bufsize;
    WAVEFORMATEX wf;
    WAVEHDR hdr = { NULL, };

    // 웨이브 파일을 연다.
    hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        puts("file not found");
        return -1;
    }

    // 재생 장치를 연다.
    wf.cbSize = sizeof(WAVEFORMATEX);
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = 2;
    wf.wBitsPerSample = 16;
    wf.nSamplesPerSec = 44100;
    wf.nBlockAlign = wf.nChannels * wf.wBitsPerSample / 8;
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
    waveOutOpen(&hWaveDev, WAVE_MAPPER, &wf, (DWORD)NULL, 0, CALLBACK_NULL);
    SetFilePointer(hFile, 44, NULL, SEEK_SET);

    bufsize = wf.nAvgBytesPerSec;
    samplebuf = (char*)malloc(bufsize);
    hdr.lpData = samplebuf;

    ft = GetTicks();
    et = ft;
    ui64 total = 0;
    ui32 ii = 0;
    do {
        BOOL succ = ReadFile(hFile, samplebuf, bufsize, &dwRead, NULL);
        if (succ) {
            printf("Read %d %d\n", dwRead, et - ft);
            hdr.dwBufferLength = dwRead;
            et = GetTicks();
            while (et - ft < QUERYPERFORMANCE_HZ - 1100) {
                et = GetTicks();
            }
            waveOutPrepareHeader(hWaveDev, &hdr, sizeof(WAVEHDR));
            waveOutWrite(hWaveDev, &hdr, sizeof(WAVEHDR));
            ft = GetTicks();
        }
    } while (dwRead == bufsize);

    total /= ii;
    cout << total << endl;

    waveOutUnprepareHeader(hWaveDev, &hdr, sizeof(WAVEHDR));

    free(samplebuf);

    waveOutClose(hWaveDev);

    CloseHandle(hFile);

    return 0;
}

int PlayWaveData(WaveDataStruct wd) {
    HWAVEOUT hWaveDev;
    HANDLE hFile;
    DWORD filesize;
    DWORD dwRead;
    char* samplebuf;
    WAVEFORMATEX wf;
    WAVEHDR hdr = { NULL, };
    hdr.dwLoops = -1; // 무한 루프?

    // 재생 장치를 연다.
    wf.cbSize = sizeof(WAVEFORMATEX);
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = 2;
    wf.wBitsPerSample = 16;
    wf.nSamplesPerSec = 44100;
    wf.nBlockAlign = wf.nChannels * wf.wBitsPerSample / 8;
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
    waveOutOpen(&hWaveDev, WAVE_MAPPER, &wf, (DWORD)NULL, 0, CALLBACK_NULL);

    // 헤더에 버퍼와 길이를 지정한다.
    hdr.lpData = (char*)wd.SampleBuffer;
    hdr.dwBufferLength = wd.bufferSize * 4;

    // 준비 및 출력한다.
    waveOutPrepareHeader(hWaveDev, &hdr, sizeof(WAVEHDR));
    waveOutWrite(hWaveDev, &hdr, sizeof(WAVEHDR));

    cout << "PlayStart" << endl;

    // 다 재생할 때까지 대기한다.
    while (waveOutUnprepareHeader(hWaveDev, &hdr, sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) {
        Sleep(100);
        MMTIME pivot;
        waveOutGetPosition(hWaveDev, &pivot, sizeof(MMTIME));
        cout << pivot.u.sample << endl;//sample = 1/44100 second. in mono sound 
    }
    // 뒷정리한다.
    waveOutUnprepareHeader(hWaveDev, &hdr, sizeof(WAVEHDR));

    waveOutClose(hWaveDev);

    return 0;
}

int PlayWaveStream(WaveDataStruct wd) {
    constexpr ui32 seclen = updateSampleCount * 4;
    constexpr ui32 update_sample_count = updateSampleCount;
    HWAVEOUT hWaveDev;
    HANDLE hFile;
    DWORD filesize;
    DWORD dwRead;
    char* samplebuf;
    WAVEFORMATEX wf;
    WAVEHDR hdr[2] = {};
    hdr[0].dwLoops = -1;
    hdr[1].dwLoops = -1;

    // 재생 장치를 연다.
    wf.cbSize = sizeof(WAVEFORMATEX);
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = 2;
    wf.wBitsPerSample = 16;
    wf.nSamplesPerSec = 44100;
    wf.nBlockAlign = wf.nChannels * wf.wBitsPerSample / 8;
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
    waveOutOpen(&hWaveDev, WAVE_MAPPER, &wf, (DWORD)NULL, 0, CALLBACK_NULL);

    // 헤더에 버퍼와 길이를 지정한다.
    hdr[0].lpData = (char*)wd.SampleBuffer;
    hdr[0].dwBufferLength = seclen;

    hdr[1].lpData = hdr[0].lpData - seclen;
    hdr[1].dwBufferLength = seclen;

    ui64 ft = GetTicks();
    ui64 et = GetTicks();
    ui32 pivot = (ui32)0 - updateSampleCount;
    ui32 deltaTime = (ui32)((float)updateSampleCount * (float)QUERYPERFORMANCE_HZ / 44100.0f);
    while (true) {
        // 준비 및 출력한다.
        waveOutPrepareHeader(hWaveDev, &hdr[0], sizeof(WAVEHDR));
        et = GetTicks();
        while (et - ft < deltaTime) {
            et = GetTicks();
        }
        waveOutWrite(hWaveDev, &hdr[0], sizeof(WAVEHDR));
        ft = GetTicks();
        pivot += updateSampleCount;
        while (waveOutUnprepareHeader(hWaveDev, &hdr[1], sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) {
        }
        waveOutUnprepareHeader(hWaveDev, &hdr[0], sizeof(WAVEHDR));
        if (hdr[1].lpData + 3 * seclen < ((char*)wd.SampleBuffer) + 4 * wd.bufferSize) {
            hdr[1].lpData += 2 * seclen;
        }
        else {
            break;
        }
        waveOutPrepareHeader(hWaveDev, &hdr[1], sizeof(WAVEHDR));

        et = GetTicks();
        while (et - ft < deltaTime) {
            et = GetTicks();
        }
        waveOutWrite(hWaveDev, &hdr[1], sizeof(WAVEHDR));
        ft = GetTicks();
        pivot += updateSampleCount;
        // 다 재생할 때까지 대기한다. hdr 0
        while (waveOutUnprepareHeader(hWaveDev, &hdr[0], sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) {
        }
        // 뒷정리한다.
        waveOutUnprepareHeader(hWaveDev, &hdr[0], sizeof(WAVEHDR));
        if (hdr[0].lpData + 3 * seclen < (char*)wd.SampleBuffer + 4 * wd.bufferSize) {
            hdr[0].lpData += 2 * seclen;
        }
        else {
            break;
        }
    }

    while (waveOutUnprepareHeader(hWaveDev, &hdr[0], sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) {
    }
    while (waveOutUnprepareHeader(hWaveDev, &hdr[1], sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) {
    }
    waveOutUnprepareHeader(hWaveDev, &hdr[0], sizeof(WAVEHDR));
    waveOutUnprepareHeader(hWaveDev, &hdr[1], sizeof(WAVEHDR));
    waveOutClose(hWaveDev);

    return 0;
}

int PlayWaveStream_Nheader(WaveDataStruct wd, ui32 headerCount, ui32 LoopNum) {
    constexpr ui32 seclen = updateSampleCount * 4;
    constexpr ui32 update_sample_count = updateSampleCount;
    HWAVEOUT hWaveDev;
    HANDLE hFile;
    DWORD filesize;
    DWORD dwRead;
    char* samplebuf;
    WAVEFORMATEX wf;
    WAVEHDR hdr[128] = {};
    int loopstack = LoopNum;
    for (int i = 0; i < headerCount; ++i) {
        hdr[i].dwLoops = -1;
    }

    // 재생 장치를 연다.
    wf.cbSize = sizeof(WAVEFORMATEX);
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = 2;
    wf.wBitsPerSample = 16;
    wf.nSamplesPerSec = 44100;
    wf.nBlockAlign = wf.nChannels * wf.wBitsPerSample / 8;
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
    waveOutOpen(&hWaveDev, WAVE_MAPPER, &wf, (DWORD)NULL, 0, CALLBACK_NULL);

    // 헤더에 버퍼와 길이를 지정한다.
    hdr[0].lpData = (char*)wd.SampleBuffer;
    hdr[0].dwBufferLength = seclen;

    for (int i = 1; i < headerCount; ++i) {
        hdr[i].lpData = hdr[0].lpData - (headerCount - i) * seclen;
        hdr[i].dwBufferLength = seclen;
    }

    /*hdr[1].lpData = hdr[0].lpData - 3 * seclen;
    hdr[1].dwBufferLength = seclen;

    hdr[2].lpData = hdr[0].lpData - 2 * seclen;
    hdr[2].dwBufferLength = seclen;

    hdr[3].lpData = hdr[0].lpData - seclen;
    hdr[3].dwBufferLength = seclen;*/

    ui32 niarr[128] = {};
    for (int i = 0; i < headerCount; ++i) {
        niarr[i] = (i + 1) % headerCount;
    }

    ui64 ft = GetTicks();
    ui64 et = GetTicks();
    ui32 pivot = (ui32)0 - updateSampleCount;
    ui32 deltaTime = (ui32)((float)updateSampleCount * (float)QUERYPERFORMANCE_HZ / 44100.0f);

    while (true) {
        // 준비 및 출력한다.
        for (int i = 0; i < headerCount; ++i) {
            int ni = niarr[i];

            waveOutPrepareHeader(hWaveDev, &hdr[i], sizeof(WAVEHDR));
            et = GetTicks();
            while (et - ft < deltaTime) {
                et = GetTicks();
            }
            waveOutWrite(hWaveDev, &hdr[i], sizeof(WAVEHDR));
            ft = GetTicks();
            pivot += updateSampleCount;
            while (waveOutUnprepareHeader(hWaveDev, &hdr[ni], sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) {
            }
            waveOutUnprepareHeader(hWaveDev, &hdr[ni], sizeof(WAVEHDR));
            if (hdr[ni].lpData + (headerCount + 1) * seclen < ((char*)wd.SampleBuffer) + 4 * wd.bufferSize) {
                hdr[ni].lpData += headerCount * seclen;
            }
            else {
                loopstack -= 1;
                if (loopstack > 0) {
                    hdr[0].lpData = (char*)wd.SampleBuffer;
                    hdr[0].dwBufferLength = seclen;

                    for (int i = 1; i < headerCount; ++i) {
                        hdr[i].lpData = hdr[0].lpData - (headerCount - i) * seclen;
                        hdr[i].dwBufferLength = seclen;
                    }

                    pivot = 0;
                    ft = GetTicks() - deltaTime;

                    break;
                }
                else {
                    goto WAVESTREAMING_FINISH;
                }
            }
        }
    }

WAVESTREAMING_FINISH:

    for (int i = 0; i < headerCount; ++i) {
        while (waveOutUnprepareHeader(hWaveDev, &hdr[i], sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) {
        }
    }
    for (int i = 0; i < headerCount; ++i) {
        waveOutUnprepareHeader(hWaveDev, &hdr[i], sizeof(WAVEHDR));
    }

    waveOutClose(hWaveDev);

    return 0;
}