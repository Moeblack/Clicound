/* dialog.h - 设置对话框模块
 * 模态对话框，用户点确定时更新 cfg 并返回 TRUE。
 */
#pragma once
#include <windows.h>
#include "config.h"

BOOL Dialog_Show(HWND hwndParent, HINSTANCE hInst, AppConfig* cfg);
