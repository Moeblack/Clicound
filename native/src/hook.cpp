/* hook.cpp - WH_MOUSE_LL 钩子实现
 *
 * 关键设计决策：
 * 1. 点击防抖：同一按钮 30ms 内不重复触发，避免双击叠音
 * 2. 滚轮节流：累积 delta + 最小间隔双重门控
 *    - 传统鼠标每格 120 delta，精密触摸板可能发送 30/60 小增量
 *    - 累积达到阈值且距离上次播放超过最小间隔才触发
 * 3. delta 从 MSLLHOOKSTRUCT.mouseData 高 16 位取，转 signed short
 *
 * Python 版踩过的坑：关闭终端后钩子残留。
 * C++ 版在 WM_DESTROY 中调用 Hook_Uninstall 确保卸载。
 */
#include "hook.h"

/* ---- 全局状态 ---- */
static HHOOK              g_mouseHook   = NULL;
static MouseEventCallback g_callback    = NULL;
static BOOL               g_enabled     = TRUE;
static BOOL               g_triggerOnPress = TRUE;

/* 点击防抖：每个按钮记录上次触发时间 */
static DWORD g_lastClickTime[3] = {};  /* left=0, middle=1, right=2 */
static const DWORD CLICK_DEBOUNCE_MS = 30;

/* 滚轮节流状态 */
static int   g_accumulatedDelta     = 0;
static DWORD g_lastScrollSoundTime  = 0;
static int   g_scrollDeltaThreshold = 60;   /* 默认半个 WHEEL_DELTA */
static int   g_scrollMinInterval    = 25;   /* ms，最大约 40 次/秒 */

/* ---- 钩子回调 ---- */

static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode < 0 || !g_enabled || !g_callback) {
        return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
    }

    MSLLHOOKSTRUCT* pMouse = (MSLLHOOKSTRUCT*)lParam;
    DWORD now = GetTickCount();

    /* ---- 点击事件 ---- */
    int btnIndex = -1;
    int eventType = -1;
    BOOL isDown = FALSE;

    switch (wParam) {
    case WM_LBUTTONDOWN: btnIndex = 0; eventType = MOUSE_EVENT_LEFT;   isDown = TRUE;  break;
    case WM_LBUTTONUP:   btnIndex = 0; eventType = MOUSE_EVENT_LEFT;   isDown = FALSE; break;
    case WM_MBUTTONDOWN: btnIndex = 1; eventType = MOUSE_EVENT_MIDDLE; isDown = TRUE;  break;
    case WM_MBUTTONUP:   btnIndex = 1; eventType = MOUSE_EVENT_MIDDLE; isDown = FALSE; break;
    case WM_RBUTTONDOWN: btnIndex = 2; eventType = MOUSE_EVENT_RIGHT;  isDown = TRUE;  break;
    case WM_RBUTTONUP:   btnIndex = 2; eventType = MOUSE_EVENT_RIGHT;  isDown = FALSE; break;
    }

    if (btnIndex >= 0) {
        /* 根据触发模式决定是否响应 */
        BOOL shouldTrigger = (g_triggerOnPress && isDown) || (!g_triggerOnPress && !isDown);
        if (shouldTrigger) {
            /* 防抖：同一按钮 CLICK_DEBOUNCE_MS 内不重复触发 */
            if (now - g_lastClickTime[btnIndex] >= CLICK_DEBOUNCE_MS) {
                g_callback(eventType);
                g_lastClickTime[btnIndex] = now;
            }
        }
        return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
    }

    /* ---- 滚轮事件 ---- */
    if (wParam == WM_MOUSEWHEEL) {
        /* mouseData 高 16 位是滚轮增量，带符号
         * 正值 = 向上（远离用户），负值 = 向下（靠近用户） */
        short delta = (short)HIWORD(pMouse->mouseData);
        g_accumulatedDelta += delta;

        /* 双重门控：累积量达到阈值 AND 距离上次播放超过最小间隔 */
        int absDelta = g_accumulatedDelta < 0 ? -g_accumulatedDelta : g_accumulatedDelta;
        if (absDelta >= g_scrollDeltaThreshold &&
            (int)(now - g_lastScrollSoundTime) >= g_scrollMinInterval) {
            if (g_accumulatedDelta > 0) {
                g_callback(MOUSE_EVENT_SCROLL_UP);
            } else {
                g_callback(MOUSE_EVENT_SCROLL_DOWN);
            }
            g_accumulatedDelta = 0;
            g_lastScrollSoundTime = now;
        }
    }

    /* 始终传递事件，不拦截其他应用的鼠标输入 */
    return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
}

/* ---- 公开接口 ---- */

BOOL Hook_Install(HINSTANCE hInst, MouseEventCallback callback) {
    g_callback = callback;
    g_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, hInst, 0);
    return (g_mouseHook != NULL);
}

void Hook_Uninstall() {
    if (g_mouseHook) {
        UnhookWindowsHookEx(g_mouseHook);
        g_mouseHook = NULL;
    }
}

void Hook_SetEnabled(BOOL enabled) {
    g_enabled = enabled;
}

void Hook_SetTriggerOnPress(BOOL onPress) {
    g_triggerOnPress = onPress;
}

void Hook_SetScrollThrottle(int deltaThreshold, int minIntervalMs) {
    g_scrollDeltaThreshold = deltaThreshold;
    g_scrollMinInterval = minIntervalMs;
}
