/* config.cpp - 配置文件读写实现
 * 用 cJSON 解析/生成 JSON，内部做 UTF-8 <-> wchar_t 互转。
 * 相对路径以 EXE 所在目录为基准。
 */
#include "config.h"
#include "cJSON.h"
#include <stdio.h>
#include <shlwapi.h>  /* PathRemoveFileSpecW, PathAppendW */
#pragma comment(lib, "shlwapi.lib")

/* ---- UTF-8 / wchar_t 工具函数 ---- */

static void Utf8ToWide(const char* utf8, wchar_t* wide, int wideSize) {
    if (!utf8 || !wide) return;
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, wideSize);
}

static void WideToUtf8(const wchar_t* wide, char* utf8, int utf8Size) {
    if (!wide || !utf8) return;
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, utf8Size, NULL, NULL);
}

/* ---- EXE 所在目录 ---- */

static void GetExeDir(wchar_t* buf, int bufSize) {
    GetModuleFileNameW(NULL, buf, bufSize);
    /* 去掉文件名，只保留目录 */
    PathRemoveFileSpecW(buf);
}

/* 把相对路径转为绝对路径（基于 EXE 目录） */
static void ResolveRelativePath(const wchar_t* relPath, wchar_t* absPath, int absSize) {
    wchar_t exeDir[MAX_PATH];
    GetExeDir(exeDir, MAX_PATH);
    wcscpy_s(absPath, absSize, exeDir);
    PathAppendW(absPath, relPath);
}

/* 把绝对路径转为相对于 EXE 目录的相对路径（如果可能） */
static void MakeRelativePath(const wchar_t* absPath, wchar_t* relPath, int relSize) {
    wchar_t exeDir[MAX_PATH];
    GetExeDir(exeDir, MAX_PATH);
    /* 确保目录以反斜杠结尾 */
    size_t len = wcslen(exeDir);
    if (len > 0 && exeDir[len - 1] != L'\\') {
        wcscat_s(exeDir, MAX_PATH, L"\\");
    }
    /* 如果 absPath 以 exeDir 开头，截取相对部分 */
    if (_wcsnicmp(absPath, exeDir, wcslen(exeDir)) == 0) {
        wcscpy_s(relPath, relSize, absPath + wcslen(exeDir));
    } else {
        wcscpy_s(relPath, relSize, absPath);
    }
}

/* ---- 接口实现 ---- */

void Config_GetPath(wchar_t* buf, int bufSize) {
    GetExeDir(buf, bufSize);
    PathAppendW(buf, L"config.json");
}

void Config_SetDefaults(AppConfig* cfg) {
    ZeroMemory(cfg, sizeof(AppConfig));
    /* 默认音效路径：相对于 EXE 目录的 sounds/ 子目录 */
    ResolveRelativePath(L"sounds\\left_click.wav",   cfg->left_click_path,   MAX_PATH);
    ResolveRelativePath(L"sounds\\middle_click.wav", cfg->middle_click_path, MAX_PATH);
    ResolveRelativePath(L"sounds\\right_click.wav",  cfg->right_click_path,  MAX_PATH);
    ResolveRelativePath(L"sounds\\scroll_up.wav",    cfg->scroll_up_path,    MAX_PATH);
    ResolveRelativePath(L"sounds\\scroll_down.wav",  cfg->scroll_down_path,  MAX_PATH);

    cfg->enable_left   = TRUE;
    cfg->enable_middle = TRUE;
    cfg->enable_right  = TRUE;
    cfg->enable_scroll = TRUE;

    cfg->volume             = 70;
    cfg->scroll_sensitivity = 60;   /* 半个 WHEEL_DELTA，折中默认 */
    cfg->trigger_on_press   = TRUE;
    cfg->scroll_throttle_ms = 25;   /* 最大 ~40 次/秒 */
}

/* 从 cJSON 取字符串并转成绝对路径 */
static void ReadPathField(cJSON* root, const char* key, wchar_t* dest, int destSize) {
    cJSON* item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (cJSON_IsString(item) && item->valuestring) {
        wchar_t widePath[MAX_PATH];
        Utf8ToWide(item->valuestring, widePath, MAX_PATH);
        /* 如果是相对路径则转绝对 */
        if (!PathIsRelativeW(widePath)) {
            wcscpy_s(dest, destSize, widePath);
        } else {
            ResolveRelativePath(widePath, dest, destSize);
        }
    }
}

void Config_Load(AppConfig* cfg) {
    Config_SetDefaults(cfg);

    wchar_t cfgPath[MAX_PATH];
    Config_GetPath(cfgPath, MAX_PATH);

    /* 读取整个文件到内存 */
    HANDLE hFile = CreateFileW(cfgPath, GENERIC_READ, FILE_SHARE_READ,
                               NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;  /* 文件不存在，用默认值 */

    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == 0 || fileSize == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        return;
    }

    char* jsonBuf = (char*)malloc(fileSize + 1);
    DWORD bytesRead = 0;
    ReadFile(hFile, jsonBuf, fileSize, &bytesRead, NULL);
    CloseHandle(hFile);
    jsonBuf[bytesRead] = '\0';

    cJSON* root = cJSON_Parse(jsonBuf);
    free(jsonBuf);
    if (!root) return;

    /* 解析各字段 */
    ReadPathField(root, "left_click",   cfg->left_click_path,   MAX_PATH);
    ReadPathField(root, "middle_click", cfg->middle_click_path, MAX_PATH);
    ReadPathField(root, "right_click",  cfg->right_click_path,  MAX_PATH);
    ReadPathField(root, "scroll_up",    cfg->scroll_up_path,    MAX_PATH);
    ReadPathField(root, "scroll_down",  cfg->scroll_down_path,  MAX_PATH);

    cJSON* item;
    item = cJSON_GetObjectItemCaseSensitive(root, "volume");
    if (cJSON_IsNumber(item)) cfg->volume = item->valueint;

    item = cJSON_GetObjectItemCaseSensitive(root, "enable_left");
    if (cJSON_IsBool(item)) cfg->enable_left = cJSON_IsTrue(item);
    item = cJSON_GetObjectItemCaseSensitive(root, "enable_middle");
    if (cJSON_IsBool(item)) cfg->enable_middle = cJSON_IsTrue(item);
    item = cJSON_GetObjectItemCaseSensitive(root, "enable_right");
    if (cJSON_IsBool(item)) cfg->enable_right = cJSON_IsTrue(item);
    item = cJSON_GetObjectItemCaseSensitive(root, "enable_scroll");
    if (cJSON_IsBool(item)) cfg->enable_scroll = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(root, "scroll_sensitivity");
    if (cJSON_IsNumber(item)) cfg->scroll_sensitivity = item->valueint;
    item = cJSON_GetObjectItemCaseSensitive(root, "scroll_throttle_ms");
    if (cJSON_IsNumber(item)) cfg->scroll_throttle_ms = item->valueint;
    item = cJSON_GetObjectItemCaseSensitive(root, "trigger_on_press");
    if (cJSON_IsBool(item)) cfg->trigger_on_press = cJSON_IsTrue(item);

    cJSON_Delete(root);
}

void Config_Save(const AppConfig* cfg) {
    cJSON* root = cJSON_CreateObject();

    /* 将绝对路径转回相对路径再存储，保持配置文件简洁可移植 */
    wchar_t relW[MAX_PATH]; char relA[MAX_PATH * 2];

    #define SAVE_PATH(key, field) \
        MakeRelativePath(cfg->field, relW, MAX_PATH); \
        WideToUtf8(relW, relA, sizeof(relA)); \
        /* 把反斜杠转正斜杠，JSON 读起来更自然 */ \
        for (char* p = relA; *p; ++p) { if (*p == '\\') *p = '/'; } \
        cJSON_AddStringToObject(root, key, relA);

    SAVE_PATH("left_click",   left_click_path)
    SAVE_PATH("middle_click", middle_click_path)
    SAVE_PATH("right_click",  right_click_path)
    SAVE_PATH("scroll_up",    scroll_up_path)
    SAVE_PATH("scroll_down",  scroll_down_path)
    #undef SAVE_PATH

    cJSON_AddNumberToObject(root, "volume", cfg->volume);
    cJSON_AddBoolToObject(root, "enable_left",   cfg->enable_left);
    cJSON_AddBoolToObject(root, "enable_middle", cfg->enable_middle);
    cJSON_AddBoolToObject(root, "enable_right",  cfg->enable_right);
    cJSON_AddBoolToObject(root, "enable_scroll", cfg->enable_scroll);
    cJSON_AddNumberToObject(root, "scroll_sensitivity", cfg->scroll_sensitivity);
    cJSON_AddNumberToObject(root, "scroll_throttle_ms", cfg->scroll_throttle_ms);
    cJSON_AddBoolToObject(root, "trigger_on_press", cfg->trigger_on_press);

    char* jsonStr = cJSON_Print(root);
    cJSON_Delete(root);

    wchar_t cfgPath[MAX_PATH];
    Config_GetPath(cfgPath, MAX_PATH);

    HANDLE hFile = CreateFileW(cfgPath, GENERIC_WRITE, 0,
                               NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written;
        /* 写入 UTF-8 BOM 是可选的，这里不写，cJSON 输出纯 ASCII/UTF-8 */
        WriteFile(hFile, jsonStr, (DWORD)strlen(jsonStr), &written, NULL);
        CloseHandle(hFile);
    }
    cJSON_free(jsonStr);
}
