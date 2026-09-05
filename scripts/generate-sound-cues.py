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


def write_stereo(name: str, mono: list[float]) -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    with wave.open(str(OUTPUT / name), "wb") as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(SAMPLE_RATE)
        for sample in mono:
            value = max(-32768, min(32767, round(sample * 32767)))
            output.writeframesraw(struct.pack("<hh", value, value))


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

    write_stereo(name, mono)


def add_reverb(samples: list[float]) -> list[float]:
    taps = [(0.065, 0.26), (0.125, 0.18), (0.215, 0.11), (0.330, 0.06)]
    wet = samples + [0.0] * int(taps[-1][0] * SAMPLE_RATE)
    for delay, gain in taps:
        offset = int(delay * SAMPLE_RATE)
        for index, sample in enumerate(samples):
            wet[index + offset] += gain * sample
    peak = max(abs(sample) for sample in wet)
    ceiling = PEAK * 0.92
    return [sample * min(1.0, ceiling / peak) for sample in wet]


def add_chime(
    samples: list[float], start: float, frequency: float, duration: float, level: float
) -> list[float]:
    offset = int(start * SAMPLE_RATE)
    count = int(duration * SAMPLE_RATE)
    if len(samples) < offset + count:
        samples.extend([0.0] * (offset + count - len(samples)))
    phase = 0.0
    for index in range(count):
        phase += 2.0 * math.pi * frequency / SAMPLE_RATE
        age = index / SAMPLE_RATE
        attack = min(1.0, age / 0.025)
        decay = math.exp(-5.5 * age / duration)
        shimmer = (
            math.sin(phase)
            + 0.24 * math.sin(0.5 * phase + 0.2)
            + 0.24 * math.sin(2.41 * phase + 0.5)
            + 0.08 * math.sin(3.03 * phase + 1.2)
        )
        samples[offset + index] += PEAK * level * attack * decay * shimmer / 1.56
    return samples


def render_drone(
    name: str,
    stages: list[Tone],
    reverb: bool = False,
    attack: float = 0.035,
    release: float = 0.14,
    chime: tuple[float, float, float, float] | None = None,
) -> None:
    total_samples = sum(int(duration * SAMPLE_RATE) for _, _, duration, _ in stages)
    mono: list[float] = []
    phase = 0.0
    elapsed = 0
    for start_frequency, end_frequency, duration, level in stages:
        count = int(duration * SAMPLE_RATE)
        for index in range(count):
            progress = index / max(1, count - 1)
            frequency = start_frequency * (end_frequency / start_frequency) ** progress
            phase += 2.0 * math.pi * frequency / SAMPLE_RATE
            pulse = 0.86 + 0.14 * math.sin(2.0 * math.pi * 6.5 * elapsed / SAMPLE_RATE)
            body = (
                0.62 * math.sin(phase)
                + 0.23 * math.sin(1.503 * phase + 0.4)
                + 0.15 * math.sin(2.007 * phase + 1.1)
            )
            mono.append(PEAK * level * pulse * envelope(elapsed, total_samples, attack, release) * body)
            elapsed += 1
    if chime:
        mono = add_chime(mono, *chime)
    write_stereo(name, add_reverb(mono) if reverb else mono)


def main() -> None:
    cues = {
        "battery-insert.wav": [(260, 330, 0.18, 0.72), (0, 0, 0.05, 0), (390, 470, 0.32, 0.90)],
        "battery-remove.wav": [(470, 390, 0.18, 0.78), (0, 0, 0.05, 0), (330, 250, 0.28, 0.72)],
        "seatbox-open.wav": [(310, 390, 0.12, 0.62), (0, 0, 0.04, 0), (430, 500, 0.16, 0.70)],
        "seatbox-closed.wav": [(500, 430, 0.12, 0.62), (0, 0, 0.04, 0), (390, 310, 0.16, 0.70)],
        "state-shutdown.wav": [(420, 330, 0.20, 0.68), (0, 0, 0.04, 0), (300, 190, 0.40, 0.82)],
        "indicator-on.wav": [(520, 500, 0.09, 0.68)],
        "indicator-off.wav": [(390, 370, 0.09, 0.58)],
        "notification-info.wav": [(410, 470, 0.18, 0.70)],
        "notification-success.wav": [(360, 430, 0.14, 0.70), (0, 0, 0.04, 0), (520, 620, 0.24, 0.84)],
        "notification-warning.wav": [(330, 300, 0.14, 0.76), (0, 0, 0.07, 0), (330, 300, 0.20, 0.76)],
        "notification-error.wav": [(290, 250, 0.18, 0.82), (0, 0, 0.04, 0), (230, 190, 0.30, 0.88)],
    }
    drones = {
        "state-wake.wav": [(115, 125, 0.65, 0.38), (125, 220, 0.55, 0.90), (220, 220, 1.15, 0.76)],
        "state-ready.wav": [(190, 190, 0.20, 0.32), (190, 330, 0.25, 0.76), (330, 330, 0.35, 0.56)],
        "state-parked.wav": [(460, 350, 0.25, 0.75), (350, 180, 0.65, 0.88), (180, 150, 0.22, 0.55)],
    }
    for name, tones in cues.items():
        render(name, tones)
    for name, stages in drones.items():
        if name == "state-wake.wav":
            render_drone(name, stages, reverb=True, attack=0.48, release=0.65)
        elif name == "state-ready.wav":
            render_drone(
                name,
                stages,
                reverb=True,
                attack=0.48,
                release=0.34,
                chime=(0.45, 560, 0.35, 0.23),
            )
        else:
            render_drone(name, stages)


if __name__ == "__main__":
    main()
