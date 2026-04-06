/* audio.h - XAudio2 低延迟音频引擎
 * 预加载 WAV 文件到内存 buffer，播放时只需 SubmitSourceBuffer，延迟 < 5ms。
 * 使用 voice pool 避免每次创建/销毁 SourceVoice。
 */
#pragma once
#include <windows.h>

/* 音效槽位枚举，对应 5 个音效文件 */
enum SoundSlot {
    SOUND_LEFT_CLICK = 0,
    SOUND_MIDDLE_CLICK,
    SOUND_RIGHT_CLICK,
    SOUND_SCROLL_UP,
    SOUND_SCROLL_DOWN,
    SOUND_COUNT
};

/* 初始化 XAudio2 引擎（含 COM 初始化） */
BOOL Audio_Init();

/* 加载 WAV 文件到指定槽位（预加载 PCM 数据到内存） */
BOOL Audio_LoadWav(SoundSlot slot, const wchar_t* filePath);

/* 播放指定槽位的音效（非阻塞，立即返回） */
void Audio_Play(SoundSlot slot);

/* 设置全局音量 0.0~1.0 */
void Audio_SetVolume(float volume);

/* 释放所有资源 */
void Audio_Shutdown();
