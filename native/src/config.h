/* config.h - 应用配置结构体与读写接口
 * 统一管理音效路径、启用开关、音量、滚轮参数等持久化配置。
 * 使用 cJSON 解析 config.json，与 Python 版格式保持一致。
 */
#pragma once
#include <windows.h>

struct AppConfig {
    wchar_t left_click_path[MAX_PATH];      /* 左键音效文件 */
    wchar_t middle_click_path[MAX_PATH];    /* 中键音效文件 */
    wchar_t right_click_path[MAX_PATH];     /* 右键音效文件 */
    wchar_t scroll_up_path[MAX_PATH];       /* 滚轮向上音效 */
    wchar_t scroll_down_path[MAX_PATH];     /* 滚轮向下音效 */

    BOOL enable_left;       /* 是否启用左键音效 */
    BOOL enable_middle;
    BOOL enable_right;
    BOOL enable_scroll;     /* 滚轮音效总开关 */

    int volume;             /* 0-100 缩放到 XAudio2 的 0.0~1.0 */
    int scroll_sensitivity; /* 30-120，对应滚轮节流的累积阈值，值越小越灵敏 */
    BOOL trigger_on_press;  /* TRUE=按下时发声，FALSE=释放时发声 */
    int scroll_throttle_ms; /* 滚轮音效最小播放间隔（毫秒） */
};

/* 获取 config.json 绝对路径（EXE 同目录） */
void Config_GetPath(wchar_t* buf, int bufSize);

/* 填充默认值 */
void Config_SetDefaults(AppConfig* cfg);

/* 从 config.json 加载，文件不存在则使用默认值 */
void Config_Load(AppConfig* cfg);

/* 保存到 config.json */
void Config_Save(const AppConfig* cfg);
