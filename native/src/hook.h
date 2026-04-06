/* hook.h - WH_MOUSE_LL 全局鼠标钩子
 * 捕获点击和滚轮事件，滚轮包含累积增量 + 最小间隔双重节流。
 * 钩子回调必须快速返回（<1s），否则 Windows 会静默移除钩子。
 */
#pragma once
#include <windows.h>

/* 事件类型，与 audio.h 的 SoundSlot 一一对应 */
#define MOUSE_EVENT_LEFT       0
#define MOUSE_EVENT_MIDDLE     1
#define MOUSE_EVENT_RIGHT      2
#define MOUSE_EVENT_SCROLL_UP  3
#define MOUSE_EVENT_SCROLL_DOWN 4

typedef void (*MouseEventCallback)(int eventType);

BOOL Hook_Install(HINSTANCE hInst, MouseEventCallback callback);
void Hook_Uninstall();
void Hook_SetEnabled(BOOL enabled);
void Hook_SetTriggerOnPress(BOOL onPress);
void Hook_SetScrollThrottle(int deltaThreshold, int minIntervalMs);
