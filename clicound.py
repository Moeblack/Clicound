# /// script
# requires-python = ">=3.11"
# dependencies = ["pynput", "pygame"]
# ///
"""
Clicound - 鼠标点击音效反馈工具
====================================
为 Windows 触摸板/鼠标的左键、中键、右键点击和滚轮滚动添加即时音效反馈。

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
import signal
import sys
import time
import atexit
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
        "scroll_up": "sounds/scroll_up.wav",
        "scroll_down": "sounds/scroll_down.wav",
        "scroll_throttle_ms": 25,        # 滚轮音效最小播放间隔（毫秒）
        "scroll_delta_threshold": 60,    # 滚轮累积增量达到此值才播放一次音效
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
    返回一个字典 {"left": Sound, "middle": Sound, "right": Sound, "scroll_up": Sound, "scroll_down": Sound}
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
        "scroll_up": "scroll_up",
        "scroll_down": "scroll_down",
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
# 全局鼠标钩子：用 pynput 监听点击和滚轮事件
# ============================================================

def start_mouse_listener(sounds: dict, config: dict):
    """
    启动全局鼠标监听。
    pynput 在 Windows 上使用 WH_MOUSE_LL 低级钩子，
    可以捕获所有窗口/桌面上的鼠标点击和滚轮事件。
    """
    from pynput.mouse import Listener, Button

    # ---- 滚轮节流状态 ----
    # 用列表包装 mutable 状态，避免 nonlocal 在嵌套函数中的兼容性问题
    scroll_state = {
        "accumulated_delta": 0,       # 累积的滚轮增量
        "last_sound_time": 0.0,       # 上次播放滚轮音效的时间戳（秒）
    }
    # 从配置读取节流参数
    throttle_sec = config.get("scroll_throttle_ms", 25) / 1000.0
    delta_threshold = config.get("scroll_delta_threshold", 60)

    def on_click(x, y, button, pressed):
        """
        鼠标点击回调。
        pressed=True 表示按下，False 表示释放。
        只在按下时发声，模拟物理按键的即时触感反馈。
        """
        if not pressed:
            return

        if button == Button.left:
            snd = sounds.get("left")
        elif button == Button.middle:
            snd = sounds.get("middle")
        elif button == Button.right:
            snd = sounds.get("right")
        else:
            snd = None

        if snd:
            snd.play()

    def on_scroll(x, y, dx, dy):
        """
        滚轮回调。
        dy>0 表示向上滚动，dy<0 表示向下滚动。
        使用"累积增量 + 最小间隔"双重门控做节流：
        - 累积增量达到阈值才播放一次（避免精密触摸板小增量时过于密集）
        - 两次播放之间至少间隔 throttle_ms（限制最大频率约 40 次/秒）
        """
        # pynput 的 dy 已经是归一化的方向值（通常为 ±1），
        # 但在不同系统/驱动下可能有不同的值。
        # 我们把 dy 乘以 WHEEL_DELTA(120) 来对齐 Win32 原始行为
        scroll_state["accumulated_delta"] += dy * 120

        now = time.perf_counter()
        delta = scroll_state["accumulated_delta"]

        if abs(delta) >= delta_threshold and (now - scroll_state["last_sound_time"]) >= throttle_sec:
            if delta > 0:
                snd = sounds.get("scroll_up")
            else:
                snd = sounds.get("scroll_down")
            if snd:
                snd.play()
            scroll_state["accumulated_delta"] = 0
            scroll_state["last_sound_time"] = now

    listener = Listener(on_click=on_click, on_scroll=on_scroll)
    listener.start()
    return listener


# ============================================================
# 主入口
# ============================================================

def main():
    print("="*50)
    print("  Clicound - 鼠标点击/滚轮音效反馈")
    print("  按 Ctrl+C 退出")
    print("="*50)
    print()

    # 1. 加载配置
    config = load_config()

    # 2. 初始化音频引擎并预加载音效
    sounds = init_audio(config)

    # 3. 启动全局鼠标监听
    listener = start_mouse_listener(sounds, config)

    print()
    print("✅ 已启动！现在每次鼠标点击和滚轮滚动都会发出音效。")
    print("💡 修改 config.json 可自定义音效文件和音量。")
    print()

    # 4. 注册清理函数，确保终端被关闭、进程被杀死等任何退出方式都能卸载钩子
    #    问题背景：pynput.Listener 在后台线程跑 Win32 消息循环和 WH_MOUSE_LL 钩子，
    #    如果不显式 stop()，关闭终端窗口时钩子可能残留，进程变成无窗口后台僵尸。
    def cleanup():
        """统一清理：停止监听器 + 关闭音频引擎"""
        try:
            listener.stop()
        except Exception:
            pass
        try:
            import pygame.mixer as mixer
            mixer.quit()
        except Exception:
            pass

    atexit.register(cleanup)

    # 处理 SIGTERM（kill 命令）和 SIGINT（Ctrl+C），
    # 以及 Windows 特有的 CTRL_CLOSE_EVENT（关闭终端窗口）、CTRL_BREAK_EVENT
    def signal_handler(signum, frame):
        """收到终止信号时主动退出，触发 atexit 清理"""
        sys.exit(0)

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    # Windows 下 SIGBREAK 对应终端关闭/Ctrl+Break
    if hasattr(signal, "SIGBREAK"):
        signal.signal(signal.SIGBREAK, signal_handler)

    # 5. 保持主线程存活
    try:
        listener.join()
    except (KeyboardInterrupt, SystemExit):
        print("\n正在退出...")
        cleanup()
        print("已关闭。再见！")


if __name__ == "__main__":
    main()
