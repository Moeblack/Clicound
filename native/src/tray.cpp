/* tray.cpp - 系统托盘实现
 *
 * 托盘图标是本应用的唯一可见界面元素（没有主窗口），
 * 体现“装好就忘”的设计哲学。
 */
#include "tray.h"
#include "resource.h"
#include <shellapi.h>

#define TRAY_UID 1  /* 托盘图标 ID，同一窗口可以有多个图标，这里只用一个 */

BOOL Tray_Create(HWND hwnd, HICON hIcon) {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd   = hwnd;
    nid.uID    = TRAY_UID;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon  = hIcon;
    wcscpy_s(nid.szTip, L"Clicound - 鼠标音效反馈");
    return Shell_NotifyIconW(NIM_ADD, &nid);
}

void Tray_Remove(HWND hwnd) {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd   = hwnd;
    nid.uID    = TRAY_UID;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

void Tray_ShowContextMenu(HWND hwnd, BOOL isEnabled, BOOL isAutoStart) {
    HMENU hMenu = CreatePopupMenu();
    /* 第一项：启用/暂停，勾选状态反映当前状态 */
    AppendMenuW(hMenu, MF_STRING | (isEnabled ? MF_CHECKED : MF_UNCHECKED),
                ID_TRAY_TOGGLE, L"已启用");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_SETTINGS, L"设置...");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING | (isAutoStart ? MF_CHECKED : MF_UNCHECKED),
                ID_TRAY_AUTOSTART, L"开机自启");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"退出");

    /* 必须先 SetForegroundWindow，否则菜单不会在点击外部时自动关闭 */
    SetForegroundWindow(hwnd);
    POINT pt;
    GetCursorPos(&pt);
    TrackPopupMenu(hMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN,
                   pt.x, pt.y, 0, hwnd, NULL);
    /* TrackPopupMenu 后必须发一个空消息，否则菜单可能残留 */
    PostMessageW(hwnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

void Tray_ShowBalloon(HWND hwnd, const wchar_t* title, const wchar_t* text) {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd   = hwnd;
    nid.uID    = TRAY_UID;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO;
    wcscpy_s(nid.szInfoTitle, title);
    wcscpy_s(nid.szInfo, text);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void Tray_UpdateTooltip(HWND hwnd, const wchar_t* tip) {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd   = hwnd;
    nid.uID    = TRAY_UID;
    nid.uFlags = NIF_TIP;
    wcscpy_s(nid.szTip, tip);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}
