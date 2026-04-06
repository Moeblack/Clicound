/* dialog.cpp - 设置对话框实现
 *
 * 单页对话框，控件布局在 resource.rc 中定义。
 * 本文件只处理逻辑：初始化控件值、浏览文件、读取用户输入。
 */
#include "dialog.h"
#include "audio.h"
#include "resource.h"
#include <commdlg.h>    /* GetOpenFileNameW */
#include <commctrl.h>   /* TBM_SETRANGE, TBM_SETPOS 等 Trackbar 消息 */
#include <stdio.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

/* ---- 文件浏览对话框 ---- */

/* 弹出 WAV 文件选择对话框，成功则将路径写入 editCtrl */
static void BrowseWavFile(HWND hDlg, int editCtrlId) {
    wchar_t filePath[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = hDlg;
    ofn.lpstrFilter = L"WAV 文件\0*.wav\0所有文件\0*.*\0";
    ofn.lpstrFile   = filePath;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        SetDlgItemTextW(hDlg, editCtrlId, filePath);
    }
}

/* ---- 更新音量标签 ---- */

static void UpdateVolumeLabel(HWND hDlg) {
    int pos = (int)SendDlgItemMessageW(hDlg, IDC_SLIDER_VOLUME, TBM_GETPOS, 0, 0);
    wchar_t buf[16];
    swprintf_s(buf, L"%d%%", pos);
    SetDlgItemTextW(hDlg, IDC_LABEL_VOLUME, buf);
}

/* ---- 对话框过程 ---- */

static INT_PTR CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    /* 用 DWLP_USER 存储指向 AppConfig 的指针 */
    AppConfig* cfg = (AppConfig*)GetWindowLongPtrW(hDlg, DWLP_USER);

    switch (msg) {
    case WM_INITDIALOG: {
        cfg = (AppConfig*)lParam;
        SetWindowLongPtrW(hDlg, DWLP_USER, (LONG_PTR)cfg);

        /* 填充音效路径 */
        SetDlgItemTextW(hDlg, IDC_EDIT_LEFT,       cfg->left_click_path);
        SetDlgItemTextW(hDlg, IDC_EDIT_MIDDLE,     cfg->middle_click_path);
        SetDlgItemTextW(hDlg, IDC_EDIT_RIGHT,      cfg->right_click_path);
        SetDlgItemTextW(hDlg, IDC_EDIT_SCROLLUP,   cfg->scroll_up_path);
        SetDlgItemTextW(hDlg, IDC_EDIT_SCROLLDOWN, cfg->scroll_down_path);

        /* 启用复选框 */
        CheckDlgButton(hDlg, IDC_CHK_LEFT,   cfg->enable_left   ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CHK_MIDDLE, cfg->enable_middle ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CHK_RIGHT,  cfg->enable_right  ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CHK_SCROLL, cfg->enable_scroll ? BST_CHECKED : BST_UNCHECKED);

        /* 音量滑块：0-100 */
        SendDlgItemMessageW(hDlg, IDC_SLIDER_VOLUME, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        SendDlgItemMessageW(hDlg, IDC_SLIDER_VOLUME, TBM_SETPOS, TRUE, cfg->volume);
        UpdateVolumeLabel(hDlg);

        /* 滚轮灵敏度滑块：值越小越灵敏，但 UI 上“右=高灵敏度”
         * 内部值范围 30-120，UI 上反转：滑块位置 = 150 - 实际值
         * 这样滑块向右拖 = 灵敏度越高 = 阈值越小 = 响得越多 */
        SendDlgItemMessageW(hDlg, IDC_SLIDER_SENSITIVITY, TBM_SETRANGE, TRUE, MAKELPARAM(30, 120));
        SendDlgItemMessageW(hDlg, IDC_SLIDER_SENSITIVITY, TBM_SETPOS, TRUE, 150 - cfg->scroll_sensitivity);

        /* 触发时机单选按钮 */
        CheckRadioButton(hDlg, IDC_RADIO_PRESS, IDC_RADIO_RELEASE,
                         cfg->trigger_on_press ? IDC_RADIO_PRESS : IDC_RADIO_RELEASE);

        return TRUE;
    }

    case WM_HSCROLL: {
        /* 滑块值变化时实时更新标签 */
        HWND hCtrl = (HWND)lParam;
        if (hCtrl == GetDlgItem(hDlg, IDC_SLIDER_VOLUME)) {
            UpdateVolumeLabel(hDlg);
        }
        return TRUE;
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);

        /* 浏览按钮：弹出文件选择对话框 */
        switch (wmId) {
        case IDC_BTN_LEFT:       BrowseWavFile(hDlg, IDC_EDIT_LEFT);       return TRUE;
        case IDC_BTN_MIDDLE:     BrowseWavFile(hDlg, IDC_EDIT_MIDDLE);     return TRUE;
        case IDC_BTN_RIGHT:      BrowseWavFile(hDlg, IDC_EDIT_RIGHT);      return TRUE;
        case IDC_BTN_SCROLLUP:   BrowseWavFile(hDlg, IDC_EDIT_SCROLLUP);   return TRUE;
        case IDC_BTN_SCROLLDOWN: BrowseWavFile(hDlg, IDC_EDIT_SCROLLDOWN); return TRUE;

        case IDC_BTN_PREVIEW: {
            /* 试听：用当前滑块音量播放左键音效 */
            int vol = (int)SendDlgItemMessageW(hDlg, IDC_SLIDER_VOLUME, TBM_GETPOS, 0, 0);
            Audio_SetVolume(vol / 100.0f);
            Audio_Play(SOUND_LEFT_CLICK);
            return TRUE;
        }

        case IDOK: {
            /* 从控件读取所有值回 cfg */
            GetDlgItemTextW(hDlg, IDC_EDIT_LEFT,       cfg->left_click_path,   MAX_PATH);
            GetDlgItemTextW(hDlg, IDC_EDIT_MIDDLE,     cfg->middle_click_path, MAX_PATH);
            GetDlgItemTextW(hDlg, IDC_EDIT_RIGHT,      cfg->right_click_path,  MAX_PATH);
            GetDlgItemTextW(hDlg, IDC_EDIT_SCROLLUP,   cfg->scroll_up_path,    MAX_PATH);
            GetDlgItemTextW(hDlg, IDC_EDIT_SCROLLDOWN, cfg->scroll_down_path,  MAX_PATH);

            cfg->enable_left   = IsDlgButtonChecked(hDlg, IDC_CHK_LEFT)   == BST_CHECKED;
            cfg->enable_middle = IsDlgButtonChecked(hDlg, IDC_CHK_MIDDLE) == BST_CHECKED;
            cfg->enable_right  = IsDlgButtonChecked(hDlg, IDC_CHK_RIGHT)  == BST_CHECKED;
            cfg->enable_scroll = IsDlgButtonChecked(hDlg, IDC_CHK_SCROLL) == BST_CHECKED;

            cfg->volume = (int)SendDlgItemMessageW(hDlg, IDC_SLIDER_VOLUME, TBM_GETPOS, 0, 0);

            /* 反转灵敏度滑块值回阈值 */
            int sliderPos = (int)SendDlgItemMessageW(hDlg, IDC_SLIDER_SENSITIVITY, TBM_GETPOS, 0, 0);
            cfg->scroll_sensitivity = 150 - sliderPos;

            cfg->trigger_on_press = IsDlgButtonChecked(hDlg, IDC_RADIO_PRESS) == BST_CHECKED;

            EndDialog(hDlg, TRUE);
            return TRUE;
        }

        case IDCANCEL:
            EndDialog(hDlg, FALSE);
            return TRUE;
        }
        break;
    }

    case WM_CLOSE:
        EndDialog(hDlg, FALSE);
        return TRUE;
    }

    return FALSE;
}

/* ---- 公开接口 ---- */

BOOL Dialog_Show(HWND hwndParent, HINSTANCE hInst, AppConfig* cfg) {
    INT_PTR result = DialogBoxParamW(
        hInst,
        MAKEINTRESOURCEW(IDD_SETTINGS),
        hwndParent,
        DlgProc,
        (LPARAM)cfg
    );
    return (result == TRUE);
}
