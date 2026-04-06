/* audio.cpp - XAudio2 音频引擎实现
 *
 * 核心设计：
 * 1. 启动时预加载所有 WAV PCM 数据到内存 buffer
 * 2. 预创建 voice pool（8个 SourceVoice），播放时找空闲 voice 直接提交
 * 3. 这样每次 Audio_Play 只是 SubmitSourceBuffer + Start，耗时微秒级
 *
 * WAV 解析手动处理 RIFF chunk，不依赖第三方库。
 */
#include "audio.h"
#include <xaudio2.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "ole32.lib")
/* xaudio2.lib 在 CMakeLists 中链接 */

/* ---- 全局状态 ---- */
static IXAudio2*               g_xaudio      = nullptr;
static IXAudio2MasteringVoice* g_masterVoice  = nullptr;

/* 每个槽位的 PCM 数据 */
struct WavData {
    WAVEFORMATEX format;
    BYTE*        pcmData;
    DWORD        pcmSize;
    BOOL         loaded;
};
static WavData g_wavData[SOUND_COUNT] = {};

/* Voice Pool：预创建多个 SourceVoice，允许快速连点/滚轮时多声音重叠播放 */
static const int VOICE_POOL_SIZE = 8;
static IXAudio2SourceVoice* g_voicePool[VOICE_POOL_SIZE] = {};
static BOOL g_voicePoolCreated = FALSE;

/* ---- WAV 文件解析 ---- */

/* 手动解析 RIFF/WAVE 格式，提取 fmt 和 data chunk
 * 不用 mmio 等过时 API，也不引入第三方库 */
static BOOL ParseWavFile(const wchar_t* path, WAVEFORMATEX* outFmt, BYTE** outData, DWORD* outSize) {
    HANDLE hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ,
                               NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;

    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize < 44) { CloseHandle(hFile); return FALSE; }  /* WAV 最小头部 44 字节 */

    BYTE* fileBuf = (BYTE*)malloc(fileSize);
    DWORD bytesRead;
    ReadFile(hFile, fileBuf, fileSize, &bytesRead, NULL);
    CloseHandle(hFile);

    /* 验证 RIFF 头 */
    if (memcmp(fileBuf, "RIFF", 4) != 0 || memcmp(fileBuf + 8, "WAVE", 4) != 0) {
        free(fileBuf);
        return FALSE;
    }

    /* 遍历 chunk 找 fmt 和 data */
    BYTE* fmtChunk = NULL;
    BYTE* dataChunk = NULL;
    DWORD dataSize = 0;
    DWORD pos = 12;  /* 跳过 RIFF header */

    while (pos + 8 <= bytesRead) {
        char chunkId[5] = {};
        memcpy(chunkId, fileBuf + pos, 4);
        DWORD chunkSize = *(DWORD*)(fileBuf + pos + 4);

        if (strcmp(chunkId, "fmt ") == 0) {
            fmtChunk = fileBuf + pos + 8;
        } else if (strcmp(chunkId, "data") == 0) {
            dataChunk = fileBuf + pos + 8;
            dataSize = chunkSize;
        }
        /* 下一个 chunk，对齐到 2 字节边界 */
        pos += 8 + chunkSize;
        if (chunkSize & 1) pos++;  /* RIFF 规范：chunk 大小奇数时补一个字节 */
    }

    if (!fmtChunk || !dataChunk || dataSize == 0) {
        free(fileBuf);
        return FALSE;
    }

    /* 拷贝 format 信息 */
    ZeroMemory(outFmt, sizeof(WAVEFORMATEX));
    memcpy(outFmt, fmtChunk, sizeof(WAVEFORMATEX) < 16 ? sizeof(WAVEFORMATEX) : 16);
    /* WAVEFORMATEX 前 16 字节是标准 PCM 格式头，cbSize 对 PCM 可以为 0 */
    outFmt->cbSize = 0;

    /* 拷贝 PCM 数据（必须单独分配，因为 fileBuf 要释放） */
    *outData = (BYTE*)malloc(dataSize);
    memcpy(*outData, dataChunk, dataSize);
    *outSize = dataSize;

    free(fileBuf);
    return TRUE;
}

/* ---- Voice Pool 管理 ---- */

/* 创建 voice pool：用第一个已加载的音效的格式 */
static void CreateVoicePool() {
    if (g_voicePoolCreated) return;

    /* 找第一个已加载的槽位作为格式模板 */
    WAVEFORMATEX* fmt = nullptr;
    for (int i = 0; i < SOUND_COUNT; i++) {
        if (g_wavData[i].loaded) {
            fmt = &g_wavData[i].format;
            break;
        }
    }
    if (!fmt) return;  /* 没有任何音效被加载 */

    for (int i = 0; i < VOICE_POOL_SIZE; i++) {
        HRESULT hr = g_xaudio->CreateSourceVoice(&g_voicePool[i], fmt);
        if (FAILED(hr)) g_voicePool[i] = nullptr;
    }
    g_voicePoolCreated = TRUE;
}

/* 从 pool 中找一个空闲的 voice（BuffersQueued == 0 表示空闲） */
static IXAudio2SourceVoice* AcquireVoice() {
    for (int i = 0; i < VOICE_POOL_SIZE; i++) {
        if (!g_voicePool[i]) continue;
        XAUDIO2_VOICE_STATE state;
        g_voicePool[i]->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
        if (state.BuffersQueued == 0) {
            return g_voicePool[i];
        }
    }
    /* 所有 voice 都在播放，返回第一个（强制复用，因为音效都很短） */
    if (g_voicePool[0]) {
        g_voicePool[0]->Stop();
        g_voicePool[0]->FlushSourceBuffers();
        return g_voicePool[0];
    }
    return nullptr;
}

/* ---- 公开接口 ---- */

BOOL Audio_Init() {
    /* COM 初始化，多线程模式 */
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return FALSE;

    hr = XAudio2Create(&g_xaudio, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr)) return FALSE;

    hr = g_xaudio->CreateMasteringVoice(&g_masterVoice);
    if (FAILED(hr)) {
        g_xaudio->Release();
        g_xaudio = nullptr;
        return FALSE;
    }

    return TRUE;
}

BOOL Audio_LoadWav(SoundSlot slot, const wchar_t* filePath) {
    if (slot < 0 || slot >= SOUND_COUNT) return FALSE;
    if (!g_xaudio) return FALSE;

    /* 释放旧数据（支持重新加载） */
    if (g_wavData[slot].pcmData) {
        free(g_wavData[slot].pcmData);
        g_wavData[slot].pcmData = nullptr;
    }
    g_wavData[slot].loaded = FALSE;

    BOOL ok = ParseWavFile(filePath, &g_wavData[slot].format,
                           &g_wavData[slot].pcmData, &g_wavData[slot].pcmSize);
    if (ok) {
        g_wavData[slot].loaded = TRUE;
        /* 延迟创建 voice pool，确保有格式信息可用 */
        if (!g_voicePoolCreated) CreateVoicePool();
    }
    return ok;
}

void Audio_Play(SoundSlot slot) {
    if (slot < 0 || slot >= SOUND_COUNT) return;
    if (!g_wavData[slot].loaded) return;

    IXAudio2SourceVoice* voice = AcquireVoice();
    if (!voice) return;

    /* 填充 buffer 描述符，指向预加载的 PCM 数据 */
    XAUDIO2_BUFFER buf = {};
    buf.AudioBytes = g_wavData[slot].pcmSize;
    buf.pAudioData = g_wavData[slot].pcmData;
    buf.Flags      = XAUDIO2_END_OF_STREAM;

    voice->SubmitSourceBuffer(&buf);
    voice->Start(0);
}

void Audio_SetVolume(float volume) {
    if (g_masterVoice) {
        /* Clamp 到合法范围 */
        if (volume < 0.0f) volume = 0.0f;
        if (volume > 1.0f) volume = 1.0f;
        g_masterVoice->SetVolume(volume);
    }
}

void Audio_Shutdown() {
    /* 先停止并销毁所有 source voice */
    for (int i = 0; i < VOICE_POOL_SIZE; i++) {
        if (g_voicePool[i]) {
            g_voicePool[i]->Stop();
            g_voicePool[i]->FlushSourceBuffers();
            g_voicePool[i]->DestroyVoice();
            g_voicePool[i] = nullptr;
        }
    }
    g_voicePoolCreated = FALSE;

    /* 销毁 mastering voice 和 XAudio2 实例 */
    if (g_masterVoice) {
        g_masterVoice->DestroyVoice();
        g_masterVoice = nullptr;
    }
    if (g_xaudio) {
        g_xaudio->Release();
        g_xaudio = nullptr;
    }

    /* 释放所有 PCM 数据 */
    for (int i = 0; i < SOUND_COUNT; i++) {
        if (g_wavData[i].pcmData) {
            free(g_wavData[i].pcmData);
            g_wavData[i].pcmData = nullptr;
        }
        g_wavData[i].loaded = FALSE;
    }

    CoUninitialize();
}
