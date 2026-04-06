<p align="center"><img src="icon.png" width="128" alt="Clicound Logo"></p>

# Clicound

为 Windows 笔记本触摸板和鼠标的每一次点击添加即时音效反馈。

## 为什么需要它

许多 Windows 笔记本的触摸板缺少 MacBook 那样的振动马达触觉反馈。当你使用轻点代替按压的手势时，手指得不到任何物理回馈，操作感觉很空。Clicound 用一声短促的咔嗒音弥补这个缺失，每次左键、中键、右键按下的瞬间播放音效，让你确认操作已被识别。

## 技术方案

| 模块 | 作用 | 说明 |
|------|------|------|
| pynput | 全局鼠标监听 | 封装 Win32 WH_MOUSE_LL 低级钩子，捕获所有窗口上的鼠标事件 |
| pygame.mixer | 低延迟音频播放 | 预加载 WAV 到内存，512 采样缓冲区约 11ms 延迟 |
| config.json | 用户配置 | 自定义音效文件路径、音量、缓冲区大小 |

## 快速开始

### 1. 运行

```
uv run clicound.py
```

首次运行会自动安装 pynput 和 pygame 依赖。

### 2. 退出

在终端按 Ctrl+C。

## 自定义音效

把你喜欢的 WAV 文件放进 sounds/ 目录，然后编辑 config.json 指向它们：

```json
{
    "left_click": "sounds/my_left.wav",
    "middle_click": "sounds/my_middle.wav",
    "right_click": "sounds/my_right.wav",
    "volume": 0.7,
    "audio_buffer_size": 512,
    "sample_rate": 44100
}
```

各字段含义：

- left_click / middle_click / right_click：对应按键的音效文件路径，支持相对路径或绝对路径。
- volume：音量，范围 0.0 到 1.0。
- audio_buffer_size：音频缓冲区大小（采样数）。越小延迟越低，但太小可能爆音。建议 256 到 1024 之间。
- sample_rate：采样率，通常保持 44100 即可。

## 重新生成默认音效

项目自带一个合成脚本，用正弦波衰减脉冲生成三种不同音调的咔嗒声：

```
uv run generate_default_sounds.py
```

## 文件结构

```
Clicound/
  clicound.py                  # 主程序
  config.json                  # 配置文件
  generate_default_sounds.py   # 默认音效生成脚本
  sounds/
    left_click.wav             # 左键音效 1800Hz
    middle_click.wav           # 中键音效 1200Hz
    right_click.wav            # 右键音效 1400Hz
  README.md
```

## 许可

[GPL-3.0](LICENSE)
