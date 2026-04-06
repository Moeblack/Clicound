/* resource.h - Clicound 资源 ID 定义
 * 所有对话框控件、菜单项、消息的 ID 统一在此管理，
 * 避免 magic number 散布在各源文件中。
 */
#pragma once

/* ---- 图标和对话框 ---- */
#define IDI_APPICON             101
#define IDD_SETTINGS            102

/* ---- 设置对话框：音效文件路径控件 ---- */
#define IDC_EDIT_LEFT           1001
#define IDC_BTN_LEFT            1002
#define IDC_EDIT_MIDDLE         1003
#define IDC_BTN_MIDDLE          1004
#define IDC_EDIT_RIGHT          1005
#define IDC_BTN_RIGHT           1006
#define IDC_EDIT_SCROLLUP       1007
#define IDC_BTN_SCROLLUP        1008
#define IDC_EDIT_SCROLLDOWN     1009
#define IDC_BTN_SCROLLDOWN      1010

/* ---- 设置对话框：启用复选框 ---- */
#define IDC_CHK_LEFT            1011
#define IDC_CHK_MIDDLE          1012
#define IDC_CHK_RIGHT           1013
#define IDC_CHK_SCROLL          1014

/* ---- 设置对话框：音量 ---- */
#define IDC_SLIDER_VOLUME       1015
#define IDC_LABEL_VOLUME        1016
#define IDC_BTN_PREVIEW         1017

/* ---- 设置对话框：滚轮灵敏度 ---- */
#define IDC_SLIDER_SENSITIVITY  1018
#define IDC_LABEL_SENSITIVITY   1019

/* ---- 设置对话框：触发时机 ---- */
#define IDC_RADIO_PRESS         1020
#define IDC_RADIO_RELEASE       1021

/* ---- 系统托盘自定义消息 ---- */
#define WM_TRAYICON             (WM_USER + 1)

/* ---- 托盘右键菜单项 ID ---- */
#define ID_TRAY_TOGGLE          2001
#define ID_TRAY_SETTINGS        2002
#define ID_TRAY_AUTOSTART       2003
#define ID_TRAY_EXIT            2004
