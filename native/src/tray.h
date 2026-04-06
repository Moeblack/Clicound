/* tray.h - 系统托盘图标模块
 * 封装 Shell_NotifyIconW，提供托盘图标、右键菜单、气泡通知。
 */
#pragma once
#include <windows.h>

BOOL Tray_Create(HWND hwnd, HICON hIcon);
void Tray_Remove(HWND hwnd);
void Tray_ShowContextMenu(HWND hwnd, BOOL isEnabled, BOOL isAutoStart);
void Tray_ShowBalloon(HWND hwnd, const wchar_t* title, const wchar_t* text);
void Tray_UpdateTooltip(HWND hwnd, const wchar_t* tip);
