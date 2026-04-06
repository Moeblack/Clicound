# /// script
# requires-python = ">=3.11"
# dependencies = ["pynput", "pygame"]
# ///
"""
Clicound - 鼠标点击音效反馈工具
====================================
为 Windows 触摸板/鼠标的左键、中键、右键点击添加即时音效反馈。

技术方案：
- 全局鼠标钩子：pynput （封装了 Win32 WH_MOUSE_LL 低级钩子）
- 低延迟音频：pygame.mixer （预加载 WAV，小缓冲区，最小化播放延迟）
- 配置：config.json （自定义音效文件、音量、缓冲区大小）

用法：
    uv run clicound.py
    按 Ctrl+C 或关闭终端窗口退出。
"""

import json
import os
import sys
import threading
from pathlib import Path

# ============================================================
# 配置加载
# ============================================================

# 配置文件路径：与脚本同目录的 config.json
SCRIPT_DIR = Path(__file__).resolve().parent
CONFIG_PATH = SCRIPT_DIR / "config.json"

def load_config() -> dict:
    """
    加载配置文件。
    如果 config.json 不存在，使用内置默认值。
    """
    defaults = {
        "left_click": "sounds/left_click.wav",
        "middle_click": "sounds/middle_click.wav",
        "right_click": "sounds/right_click.wav",
        "volume": 0.7,
        "audio_buffer_size": 512,   # 越小延迟越低，但太小可能爆音
        "sample_rate": 44100,
    }
    if CONFIG_PATH.exists():
        try:
            with open(CONFIG_PATH, "r", encoding="utf-8") as f:
                user_cfg = json.load(f)
            # 用户配置覆盖默认值
            defaults.update({k: v for k, v in user_cfg.items() if not k.startswith("_")})
            print(f"[配置] 已加载 {CONFIG_PATH}")
        except Exception as e:
            print(f"[警告] 读取配置失败，使用默认值: {e}")
    else:
        print(f"[配置] 未找到 {CONFIG_PATH}，使用默认值")
    return defaults


# ============================================================
# 音频引擎：用 pygame.mixer 实现低延迟播放
# ============================================================

def init_audio(config: dict):
    """
    初始化 pygame.mixer，预加载所有音效文件。
    返回一个字典 {"left": Sound, "middle": Sound, "right": Sound}
    """
    import pygame.mixer as mixer

    # pre_init 设置小缓冲区以降低延迟
    # buffer=512 大约对应 ~11ms 延迟 (44100Hz)，足够快
    mixer.pre_init(
        frequency=config["sample_rate"],
        size=-16,        # 16-bit signed
        channels=1,      # 单声道即可
        buffer=config["audio_buffer_size"],
    )
    mixer.init()

    # 设置多通道，允许快速连点时重叠播放
    mixer.set_num_channels(8)

    volume = max(0.0, min(1.0, config["volume"]))

    sounds = {}
    # 映射：配置键 -> 内部标识
    key_map = {
        "left_click": "left",
        "middle_click": "middle",
        "right_click": "right",
    }
    for cfg_key, internal_key in key_map.items():
        wav_path = config.get(cfg_key, "")
        if not wav_path:
            print(f"[警告] 配置中缺少 {cfg_key}，跳过")
            continue

        # 支持相对路径（相对于脚本目录）
        full_path = Path(wav_path)
        if not full_path.is_absolute():
            full_path = SCRIPT_DIR / full_path

        if not full_path.exists():
            print(f"[警告] 音效文件不存在: {full_path}")
            continue

        try:
            snd = mixer.Sound(str(full_path))
            snd.set_volume(volume)
            sounds[internal_key] = snd
            print(f"[音频] 已加载 {internal_key}: {full_path}")
        except Exception as e:
            print(f"[错误] 加载 {full_path} 失败: {e}")

    if not sounds:
        print("[错误] 没有任何音效文件被成功加载，请检查 config.json 和 sounds/ 目录。")
        sys.exit(1)

    return sounds


# ============================================================
# 全局鼠标钩子：用 pynput 监听点击事件
# ============================================================

def start_mouse_listener(sounds: dict):
    """
    启动全局鼠标监听。
    pynput 在 Windows 上使用 WH_MOUSE_LL 低级钩子，
    可以捕获所有窗口/桌面上的鼠标事件。
    """
    from pynput.mouse import Listener, Button

    def on_click(x, y, button, pressed):
        """
        鼠标点击回调。
        pressed=True 表示按下，False 表示释放。
        我们只在按下时发声，模拟物理按键的即时触感反馈。
        """
        if not pressed:
            return  # 只处理按下事件

        # 根据按钮类型选择音效
        if button == Button.left:
            snd = sounds.get("left")
        elif button == Button.middle:
            snd = sounds.get("middle")
        elif button == Button.right:
            snd = sounds.get("right")
        else:
            snd = None  # 侧键等其他按钮，暂不处理

        if snd:
            # 在空闲通道上播放，允许多次点击音重叠
            snd.play()

    # 创建并启动监听器
    # pynput.mouse.Listener 会在单独的线程中运行消息循环
    listener = Listener(on_click=on_click)
    listener.start()
    return listener


# ============================================================
# 主入口
# ============================================================

def main():
    print("="*50)
    print("  Clicound - 鼠标点击音效反馈")
    print("  按 Ctrl+C 退出")
    print("="*50)
    print()

    # 1. 加载配置
    config = load_config()

    # 2. 初始化音频引擎并预加载音效
    sounds = init_audio(config)

    # 3. 启动全局鼠标监听
    listener = start_mouse_listener(sounds)

    print()
    print("✅ 已启动！现在每次鼠标点击都会发出音效。")
    print("💡 修改 config.json 可自定义音效文件和音量。")
    print()

    # 4. 保持主线程存活，等待 Ctrl+C
    try:
        listener.join()  # 阻塞主线程直到监听器停止
    except KeyboardInterrupt:
        print("\n正在退出...")
        listener.stop()
        import pygame.mixer as mixer
        mixer.quit()
        print("已关闭。再见！")


if __name__ == "__main__":
    main()
