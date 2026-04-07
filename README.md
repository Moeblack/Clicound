<p align="center"><img src="icon.png" width="128" alt="Clicound Logo"></p>

# Clicound

为 Windows 笔记本触摸板和鼠标的每一次点击和滚轮添加即时音效反馈。

## 为什么需要它

许多 Windows 笔记本的触摸板缺少 MacBook 那样的振动马达触觉反馈。当你使用轻点代替按压的手势时，手指得不到任何物理回馈，操作感觉很空。Clicound 用一声短促的咔嗒音弥补这个缺失，每次点击或滚轮的瞬间播放音效，让你确认操作已被识别。

## 两个版本

| | C++ 原生版（推荐） | Python 版 |
|------|-------------|----------|
| 体积 | 91 KB 单文件 EXE | 需要 Python 运行时 |
| 启动 | 瞄间 | ~1-2 秒 |
| 内存 | ~16 MB | ~30-50 MB |
| 音频延迟 | <5 ms (XAudio2) | ~11 ms (pygame) |
| 界面 | 系统托盘 + 设置对话框 | 终端窗口 |
| 开机自启 | 内置支持 | 需要额外配置 |
| 依赖 | 无 | pynput, pygame |

## 快速开始

### C++ 原生版（推荐）

从 [Releases](https://github.com/Moeblack/Clicound/releases) 下载最新的 `Clicound.zip`，解压后直接运行 `Clicound.exe`。

程序会驻留在系统托盘：

- 左键点击托盘图标 → 打开设置
- 右键点击托盘图标 → 快捷菜单（启用/暂停、开机自启、退出）

### Python 版

```
uv run clicound.py
```

首次运行会自动安装 pynput 和 pygame 依赖。按 Ctrl+C 退出。

### 从源码构建 C++ 版

需要 Visual Studio 2022 Build Tools 和 CMake。

```
cd native
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

产物在 `native/build/Release/Clicound.exe`，运行前需要把 `config.json` 和 `sounds/` 目录复制到 EXE 同级。

## 功能

- 左键、中键、右键点击音效，各自不同音调
- 滚轮音效，向上/向下不同音调，带智能节流防止蜂鸣
- 自定义 WAV 音效文件
- 音量调节（含试听）
- 滚轮灵敏度调节（适配不同触摸板/鼠标）
- 按键启用/禁用开关
- 触发时机选择（按下或释放）
- 开机自启（C++ 版）
- 单实例保护（C++ 版）

## 自定义音效

把 WAV 文件放进 `sounds/` 目录，然后通过设置对话框或编辑 `config.json` 指向它们。

默认音效参数：

| 音效 | 频率 | 时长 | 设计意图 |
|------|------|------|----------|
| 左键 | 1800 Hz | 15 ms | 清脆短促 |
| 中键 | 1200 Hz | 20 ms | 柔和 |
| 右键 | 1400 Hz | 18 ms | 与左键区分 |
| 滚轮向上 | 3200 Hz | 6 ms | 高频棘轮感 |
| 滚轮向下 | 2800 Hz | 6 ms | 略低，提供方向感 |

重新生成默认音效：

```
uv run generate_default_sounds.py
```

## 文件结构

```
Clicound/
├─ native/                        # C++ 原生版
│   ├─ src/
│   │   ├─ main.cpp               # 主入口、消息循环、生命周期
│   │   ├─ audio.cpp/h            # XAudio2 音频引擎
│   │   ├─ hook.cpp/h             # WH_MOUSE_LL 全局钩子 + 滚轮节流
│   │   ├─ tray.cpp/h             # 系统托盘图标
│   │   ├─ dialog.cpp/h           # 设置对话框
│   │   ├─ config.cpp/h           # JSON 配置读写
│   │   └─ cjson/                 # cJSON 库（嵌入）
│   ├─ res/
│   │   ├─ resource.rc/h          # 对话框模板、控件 ID
│   │   ├─ icon.ico               # 应用图标
│   │   └─ app.manifest           # DPI 感知 + 视觉样式
│   ├─ sounds/                    # 默认音效文件
│   ├─ config.json                # 默认配置
│   └─ CMakeLists.txt             # 构建脚本
├─ clicound.py                    # Python 版主程序
├─ config.json                    # Python 版配置
├─ generate_default_sounds.py     # 音效生成脚本
├─ sounds/                        # Python 版音效
└─ LICENSE                        # GPL-3.0
```

## 许可

[GPL-3.0](LICENSE)

## 社区支持

[LINUX DO](https://linux.do/)

## 开发工具

[Limcode](https://github.com/Lianues/Lim-Code)
