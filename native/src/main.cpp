/* main.cpp - Clicound 主入口
 *
 * 程序生命周期：
 * 1. 单实例检查（Named Mutex）
 * 2. 创建隐藏窗口接收托盘消息
 * 3. 加载配置 → 初始化音频 → 加载音效 → 安装钩子 → 创建托盘
 * 4. 消息循环驱动钩子回调和 UI 事件
 * 5. 退出时完整清理（Python 版踩过的坑：必须显式卸载钩子）
 */
#include <windows.h>
#include <commctrl.h>
#include "config.h"
#include "audio.h"
#include "hook.h"
#include "tray.h"
#include "dialog.h"
#include "resource.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "comctl32.lib")

/* ---- 全局状态 ---- */
static HINSTANCE g_hInst    = NULL;
static HWND      g_hwnd     = NULL;
static AppConfig g_config   = {};
static BOOL      g_isEnabled = TRUE;  /* 当前是否启用音效 */

static const wchar_t* MUTEX_NAME = L"ClicoundSingleInstanceMutex";
static const wchar_t* WND_CLASS  = L"ClicoundWndClass";
static const wchar_t* REG_KEY    = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t* REG_VALUE  = L"Clicound";

/* ---- 开机自启辅助函数 ---- */

static BOOL IsAutoStartEnabled() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return FALSE;
    DWORD type, size = 0;
    BOOL exists = (RegQueryValueExW(hKey, REG_VALUE, NULL, &type, NULL, &size) == ERROR_SUCCESS);
    RegCloseKey(hKey);
    return exists;
}

static void SetAutoStart(BOOL enable) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS)
        return;
    if (enable) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        DWORD len = (DWORD)(wcslen(exePath) + 1) * sizeof(wchar_t);
        RegSetValueExW(hKey, REG_VALUE, 0, REG_SZ, (BYTE*)exePath, len);
    } else {
        RegDeleteValueW(hKey, REG_VALUE);
    }
    RegCloseKey(hKey);
}

/* ---- 重新加载音效和参数（设置变更后调用） ---- */

static void ReloadSoundsAndParams() {
    Audio_LoadWav(SOUND_LEFT_CLICK,   g_config.left_click_path);
    Audio_LoadWav(SOUND_MIDDLE_CLICK, g_config.middle_click_path);
    Audio_LoadWav(SOUND_RIGHT_CLICK,  g_config.right_click_path);
    Audio_LoadWav(SOUND_SCROLL_UP,    g_config.scroll_up_path);
    Audio_LoadWav(SOUND_SCROLL_DOWN,  g_config.scroll_down_path);
    Audio_SetVolume(g_config.volume / 100.0f);
    Hook_SetTriggerOnPress(g_config.trigger_on_press);
    Hook_SetScrollThrottle(g_config.scroll_sensitivity, g_config.scroll_throttle_ms);
}

/* ---- 鼠标事件回调 ---- */

static void OnMouseEvent(int eventType) {
    /* 根据配置的启用开关决定是否播放
     * 这个函数在钩子回调线程中调用，必须快速返回 */
    switch (eventType) {
    case MOUSE_EVENT_LEFT:
        if (g_config.enable_left)  Audio_Play(SOUND_LEFT_CLICK);
        break;
    case MOUSE_EVENT_MIDDLE:
        if (g_config.enable_middle) Audio_Play(SOUND_MIDDLE_CLICK);
        break;
    case MOUSE_EVENT_RIGHT:
        if (g_config.enable_right) Audio_Play(SOUND_RIGHT_CLICK);
        break;
    case MOUSE_EVENT_SCROLL_UP:
        if (g_config.enable_scroll) Audio_Play(SOUND_SCROLL_UP);
        break;
    case MOUSE_EVENT_SCROLL_DOWN:
        if (g_config.enable_scroll) Audio_Play(SOUND_SCROLL_DOWN);
        break;
    }
}

/* ---- 窗口过程 ---- */

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TRAYICON: {
        /* 托盘图标事件
         * Windows 7+ 用 LOWORD(lParam) 获取鼠标消息 */
        UINT trayMsg = LOWORD(lParam);
        if (trayMsg == WM_LBUTTONUP) {
            /* 左键点击托盘 → 打开设置对话框 */
            if (Dialog_Show(hwnd, g_hInst, &g_config)) {
                Config_Save(&g_config);
                ReloadSoundsAndParams();
            }
        } else if (trayMsg == WM_RBUTTONUP) {
            /* 右键点击托盘 → 弹出菜单 */
            Tray_ShowContextMenu(hwnd, g_isEnabled, IsAutoStartEnabled());
        }
        return 0;
    }

    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case ID_TRAY_TOGGLE:
            /* 切换启用/暂停 */
            g_isEnabled = !g_isEnabled;
            Hook_SetEnabled(g_isEnabled);
            Tray_UpdateTooltip(hwnd,
                g_isEnabled ? L"Clicound - 已启用" : L"Clicound - 已暂停");
            break;

        case ID_TRAY_SETTINGS:
            if (Dialog_Show(hwnd, g_hInst, &g_config)) {
                Config_Save(&g_config);
                ReloadSoundsAndParams();
            }
            break;

        case ID_TRAY_AUTOSTART:
            /* 切换开机自启，即时生效 */
            SetAutoStart(!IsAutoStartEnabled());
            break;

        case ID_TRAY_EXIT:
            DestroyWindow(hwnd);
            break;
        }
        return 0;
    }

    case WM_QUERYENDSESSION:
        /* 系统关机时允许退出，清理在 WM_DESTROY 中完成 */
        return TRUE;

    case WM_DESTROY:
        /* 完整清理：卸载钩子 → 删除托盘 → 释放音频
         * 顺序很重要：先停钩子以免回调访问已释放的音频资源 */
        Hook_Uninstall();
        Tray_Remove(hwnd);
        Audio_Shutdown();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

/* ---- WinMain ---- */

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    g_hInst = hInstance;

    /* 1. 单实例保护：用 Named Mutex 确保只有一个实例 */
    HANDLE hMutex = CreateMutexW(NULL, TRUE, MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        /* 已有实例在运行，尝试找到它的窗口并激活 */
        HWND hExisting = FindWindowW(WND_CLASS, NULL);
        if (hExisting) {
            /* 向已有实例发送托盘左键消息，触发打开设置对话框 */
            PostMessageW(hExisting, WM_TRAYICON, 0, WM_LBUTTONUP);
        }
        CloseHandle(hMutex);
        return 0;
    }

    /* 2. 初始化 Common Controls（Trackbar 等控件需要） */
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    /* 3. 注册隐藏窗口类 */
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = WND_CLASS;
    /* 使用系统默认图标，后续可替换为自定义 .ico */
    wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    RegisterClassExW(&wc);

    /* 4. 创建隐藏窗口（仅用于接收消息，不可见） */
    g_hwnd = CreateWindowExW(0, WND_CLASS, L"Clicound", 0,
                             0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);

    /* 5. 加载配置 */
    Config_Load(&g_config);

    /* 6. 初始化音频引擎并加载音效 */
    if (!Audio_Init()) {
        MessageBoxW(NULL, L"XAudio2 初始化失败，请确保系统支持 XAudio2。",
                    L"Clicound 错误", MB_ICONERROR);
        return 1;
    }
    ReloadSoundsAndParams();

    /* 7. 安装全局鼠标钩子 */
    if (!Hook_Install(hInstance, OnMouseEvent)) {
        MessageBoxW(NULL, L"安装鼠标钩子失败。", L"Clicound 错误", MB_ICONERROR);
        Audio_Shutdown();
        return 1;
    }

    /* 8. 创建托盘图标 */
    HICON hIcon = LoadIconW(NULL, IDI_APPLICATION);
    Tray_Create(g_hwnd, hIcon);

    /* 首次启动时显示气泡通知 */
    Tray_ShowBalloon(g_hwnd, L"Clicound", L"已启动！在系统托盘中可以找到我。");

    /* 9. 消息循环
     * GetMessage 驱动 WH_MOUSE_LL 钩子回调和窗口消息处理。
     * PostQuitMessage(0) 后 GetMessage 返回 0，循环结束。 */
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CloseHandle(hMutex);
    return (int)msg.wParam;
}
