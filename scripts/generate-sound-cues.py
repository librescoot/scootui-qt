#!/usr/bin/env python3
"""Generate ScootUI's original short notification sounds."""

from __future__ import annotations

import math
import struct
import wave
from pathlib import Path

SAMPLE_RATE = 48_000
PEAK = 0.70
OUTPUT = Path(__file__).resolve().parents[1] / "assets" / "sounds"

Tone = tuple[float, float, float, float]


def envelope(index: int, length: int, attack: float = 0.008, release: float = 0.060) -> float:
    attack_samples = max(1, int(attack * SAMPLE_RATE))
    release_samples = max(1, int(release * SAMPLE_RATE))
    return min(1.0, index / attack_samples, (length - index - 1) / release_samples)


def render(name: str, tones: list[Tone]) -> None:
    mono: list[float] = []
    phase = 0.0
    for start_frequency, end_frequency, duration, level in tones:
        count = int(duration * SAMPLE_RATE)
        if start_frequency == 0:
            mono.extend([0.0] * count)
            continue
        for index in range(count):
            progress = index / max(1, count - 1)
            frequency = start_frequency + (end_frequency - start_frequency) * progress
            phase += 2.0 * math.pi * frequency / SAMPLE_RATE
            tone = math.sin(phase) + 0.12 * math.sin(2.0 * phase)
            mono.append(PEAK * level * envelope(index, count) * tone / 1.12)

    OUTPUT.mkdir(parents=True, exist_ok=True)
    with wave.open(str(OUTPUT / name), "wb") as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(SAMPLE_RATE)
        for sample in mono:
            value = max(-32768, min(32767, round(sample * 32767)))
            output.writeframesraw(struct.pack("<hh", value, value))


def main() -> None:
    cues = {
        "battery-insert.wav": [(260, 330, 0.18, 0.72), (0, 0, 0.05, 0), (390, 470, 0.32, 0.90)],
        "state-wake.wav": [(190, 260, 0.30, 0.72), (260, 390, 0.45, 0.90)],
        "state-ready.wav": [(300, 370, 0.16, 0.70), (0, 0, 0.04, 0), (430, 520, 0.16, 0.78), (0, 0, 0.04, 0), (560, 680, 0.28, 0.90)],
        "state-parked.wav": [(470, 410, 0.18, 0.72), (340, 240, 0.32, 0.84)],
        "state-shutdown.wav": [(420, 330, 0.20, 0.68), (0, 0, 0.04, 0), (300, 190, 0.40, 0.82)],
        "indicator-on.wav": [(520, 500, 0.09, 0.68)],
        "indicator-off.wav": [(390, 370, 0.09, 0.58)],
        "notification-info.wav": [(410, 470, 0.18, 0.70)],
        "notification-success.wav": [(360, 430, 0.14, 0.70), (0, 0, 0.04, 0), (520, 620, 0.24, 0.84)],
        "notification-warning.wav": [(330, 300, 0.14, 0.76), (0, 0, 0.07, 0), (330, 300, 0.20, 0.76)],
        "notification-error.wav": [(290, 250, 0.18, 0.82), (0, 0, 0.04, 0), (230, 190, 0.30, 0.88)],
    }
    for name, tones in cues.items():
        render(name, tones)


if __name__ == "__main__":
    main()
