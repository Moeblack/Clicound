# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
"""
生成默认的鼠标点击音效 WAV 文件。
用简短的正弦波脉冲模拟轻柔的咔嗒声。
左键：较高频率短促音（清脆感）
中键：中频短促音
右键：稍低频率短促音（区分左键）
"""
import wave
import struct
import math
import os

def generate_click_wav(
    filepath: str,
    freq: float = 1800.0,
    duration_ms: float = 15.0,
    volume: float = 0.3,
    sample_rate: int = 44100,
):
    """
    生成一个极短的衰减正弦波 WAV 文件，模拟咔嗒反馈音。
    - freq: 基频 Hz
    - duration_ms: 持续时间（毫秒）
    - volume: 音量 0.0~1.0
    """
    n_samples = int(sample_rate * duration_ms / 1000.0)
    samples = []
    for i in range(n_samples):
        t = i / sample_rate
        # 指数衰减包络，让声音快速消失，听起来像物理按键的"click"
        envelope = math.exp(-t * 300)
        val = volume * envelope * math.sin(2 * math.pi * freq * t)
        # 16-bit PCM 范围 [-32767, 32767]
        samples.append(int(val * 32767))

    os.makedirs(os.path.dirname(filepath) if os.path.dirname(filepath) else ".", exist_ok=True)
    with wave.open(filepath, "w") as wf:
        wf.setnchannels(1)        # 单声道
        wf.setsampwidth(2)        # 16-bit
        wf.setframerate(sample_rate)
        wf.writeframes(struct.pack(f"<{len(samples)}h", *samples))
    print(f"已生成: {filepath}  ({n_samples} samples, {duration_ms}ms, {freq}Hz)")


if __name__ == "__main__":
    # 左键：1800Hz，15ms，清脆短促
    generate_click_wav("sounds/left_click.wav",  freq=1800, duration_ms=15, volume=0.35)
    # 中键：1200Hz，20ms，稍柔和
    generate_click_wav("sounds/middle_click.wav", freq=1200, duration_ms=20, volume=0.30)
    # 右键：1400Hz，18ms，与左键区分
    generate_click_wav("sounds/right_click.wav",  freq=1400, duration_ms=18, volume=0.32)
    print("\n默认音效文件已全部生成到 sounds/ 目录。")
