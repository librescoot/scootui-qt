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
    samples: list[float],
    start: float,
    frequency: float,
    duration: float,
    level: float,
    attack_duration: float = 0.12,
    decay_rate: float = 3.2,
    brightness: float = 0.03,
) -> list[float]:
    offset = int(start * SAMPLE_RATE)
    count = int(duration * SAMPLE_RATE)
    if len(samples) < offset + count:
        samples.extend([0.0] * (offset + count - len(samples)))
    phase = 0.0
    for index in range(count):
        age = index / SAMPLE_RATE
        phase += 2.0 * math.pi * frequency / SAMPLE_RATE
        attack = math.sin(0.5 * math.pi * min(1.0, age / attack_duration)) ** 2
        decay = math.exp(-decay_rate * age / duration)
        tone = math.sin(phase) + brightness * math.sin(2.0 * phase + 0.4)
        samples[offset + index] += PEAK * level * attack * decay * tone / (1.0 + brightness)
    return samples


def render_blob(
    name: str,
    frequency: float,
    duration: float,
    level: float,
    attack: float,
    fade_start: float,
    end_tone: tuple[float, float, float, float] | None = None,
) -> None:
    count = int(duration * SAMPLE_RATE)
    mono: list[float] = []
    phase = 0.0
    for index in range(count):
        age = index / SAMPLE_RATE
        phase += 2.0 * math.pi * frequency / SAMPLE_RATE
        if age < attack:
            amplitude = math.sin(0.5 * math.pi * age / attack) ** 2
        elif age < fade_start:
            amplitude = 1.0
        else:
            progress = (age - fade_start) / (duration - fade_start)
            amplitude = math.cos(0.5 * math.pi * progress) ** 2
        pulse = 0.94 + 0.06 * math.sin(2.0 * math.pi * 5.0 * age)
        tone = (
            0.78 * math.sin(phase)
            + 0.14 * math.sin(0.5 * phase + 0.2)
            + 0.08 * math.sin(2.0 * phase + 0.4)
        )
        mono.append(PEAK * level * amplitude * pulse * tone)
    if end_tone:
        start, end_frequency, end_duration, end_level = end_tone
        offset = int(start * SAMPLE_RATE)
        count = int(end_duration * SAMPLE_RATE)
        if len(mono) < offset + count:
            mono.extend([0.0] * (offset + count - len(mono)))
        phase = 0.0
        for index in range(count):
            age = index / SAMPLE_RATE
            phase += 2.0 * math.pi * end_frequency / SAMPLE_RATE
            if age < 0.06:
                amplitude = math.sin(0.5 * math.pi * age / 0.06) ** 2
            else:
                progress = (age - 0.06) / (end_duration - 0.06)
                amplitude = math.cos(0.5 * math.pi * progress) ** 2
            mono[offset + index] += PEAK * end_level * amplitude * math.sin(phase)
    write_stereo(name, add_reverb(mono))


def render_gong(name: str, frequency: float, duration: float, level: float) -> None:
    count = int(duration * SAMPLE_RATE)
    mono: list[float] = []
    phases = [0.0, 0.0, 0.0, 0.0]
    ratios = [0.5, 1.0, 1.51, 2.13]
    weights = [0.10, 0.64, 0.18, 0.08]
    for index in range(count):
        age = index / SAMPLE_RATE
        attack = math.sin(0.5 * math.pi * min(1.0, age / 0.028)) ** 2
        decay = math.exp(-2.4 * age / duration)
        if age > duration - 0.16:
            decay *= math.cos(0.5 * math.pi * (age - duration + 0.16) / 0.16) ** 2
        body = 0.0
        for part, (ratio, weight) in enumerate(zip(ratios, weights)):
            phases[part] += 2.0 * math.pi * frequency * ratio / SAMPLE_RATE
            body += weight * math.sin(phases[part] + part * 0.3)
        mono.append(PEAK * level * attack * decay * body)
    write_stereo(name, mono)


def render_drone(
    name: str,
    stages: list[Tone],
    reverb: bool = False,
    attack: float = 0.035,
    release: float = 0.14,
    chime: tuple[float, float, float, float] | None = None,
    rounded: bool = False,
    smooth_levels: bool = False,
    fizz: bool = False,
) -> None:
    total_samples = sum(int(duration * SAMPLE_RATE) for _, _, duration, _ in stages)
    mono: list[float] = []
    phase = 0.0
    elapsed = 0
    previous_level = stages[0][3]
    for start_frequency, end_frequency, duration, level in stages:
        count = int(duration * SAMPLE_RATE)
        for index in range(count):
            progress = index / max(1, count - 1)
            frequency = start_frequency * (end_frequency / start_frequency) ** progress
            phase += 2.0 * math.pi * frequency / SAMPLE_RATE
            pulse = 0.86 + 0.14 * math.sin(2.0 * math.pi * 6.5 * elapsed / SAMPLE_RATE)
            if smooth_levels:
                blend = min(1.0, index / (0.05 * SAMPLE_RATE))
                sample_level = previous_level + (level - previous_level) * blend
            else:
                sample_level = level
            if rounded:
                body = 0.88 * math.sin(phase) + 0.12 * math.sin(2.0 * phase + 0.4)
            else:
                body = (
                    0.62 * math.sin(phase)
                    + 0.23 * math.sin(1.503 * phase + 0.4)
                    + 0.15 * math.sin(2.007 * phase + 1.1)
                )
            if fizz:
                overall = elapsed / max(1, total_samples - 1)
                fizz_age = max(0.0, (overall - 0.32) / 0.68)
                fizz_envelope = math.sin(math.pi * fizz_age) if fizz_age <= 1.0 else 0.0
                body += 0.06 * fizz_envelope * (
                    math.sin(7.13 * phase + 0.3) + math.sin(11.37 * phase + 1.0)
                )
            mono.append(PEAK * sample_level * pulse * envelope(elapsed, total_samples, attack, release) * body)
            elapsed += 1
        previous_level = level
    if chime:
        mono = add_chime(mono, *chime)
    write_stereo(name, add_reverb(mono) if reverb else mono)


def main() -> None:
    cues: dict[str, list[Tone]] = {}
    drones = {
        "scooter-unlock.wav": [(115, 125, 0.65, 0.38), (125, 220, 0.55, 0.90), (220, 220, 1.15, 0.76)],
        "battery-inserted.wav": [(105, 125, 0.22, 0.48), (125, 225, 0.48, 0.86), (225, 225, 0.38, 0.62)],
        "battery-removed.wav": [(225, 110, 0.55, 0.76), (110, 70, 0.65, 0.48)],
    }
    for name, tones in cues.items():
        render(name, tones)
    for name, stages in drones.items():
        if name == "scooter-unlock.wav":
            render_drone(name, stages, reverb=True, attack=0.48, release=0.65)
        elif name == "battery-inserted.wav":
            render_drone(
                name,
                stages,
                reverb=True,
                attack=0.10,
                release=0.36,
                smooth_levels=True,
            )
        elif name == "battery-removed.wav":
            render_drone(
                name,
                stages,
                reverb=True,
                attack=0.06,
                release=0.62,
                rounded=True,
                smooth_levels=True,
                fizz=True,
            )
        else:
            render_drone(name, stages)
    ready = add_chime([], 0, 320, 0.30, 0.15)
    ready = add_chime(ready, 0.25, 480, 0.58, 0.20)
    write_stereo("vehicle-ready-to-drive.wav", add_reverb(ready))

    parked = add_chime([], 0, 480, 0.30, 0.15)
    parked = add_chime(parked, 0.25, 320, 0.58, 0.20)
    write_stereo("vehicle-ready-to-drive-to-parked.wav", add_reverb(parked))

    shutdown = add_chime([], 0, 240, 0.34, 0.21)
    shutdown = add_chime(shutdown, 0.27, 160, 0.82, 0.27)
    write_stereo("scooter-lock.wav", add_reverb(shutdown))

    render_blob("seatbox-open.wav", 420, 0.48, 0.18, 0.10, 0.20)
    render_blob("seatbox-closed.wav", 280, 0.52, 0.18, 0.10, 0.22)

    render_gong("blinker-pulse.wav", 260, 0.50, 0.55)
    render_gong("blinker-off.wav", 195, 0.25, 0.40)

    info = add_chime([], 0, 390, 0.46, 0.80)
    write_stereo("toast-info.wav", add_reverb(info))

    success = add_chime([], 0, 329.63, 0.30, 0.65)
    success = add_chime(success, 0.24, 493.88, 0.56, 0.85)
    write_stereo("toast-success.wav", add_reverb(success))

    warning = add_chime([], 0, 260, 0.32, 0.90)
    warning = add_chime(warning, 0.38, 260, 0.36, 0.90)
    write_stereo("toast-warning.wav", add_reverb(warning))

    error = add_chime([], 0, 320, 0.17, 0.95, 0.015, 4.5, 0.16)
    error = add_chime(error, 0.19, 320, 0.17, 1.00, 0.015, 4.5, 0.16)
    error = add_chime(error, 0.38, 320, 0.23, 1.05, 0.015, 4.5, 0.16)
    write_stereo("toast-error.wav", add_reverb(error))


if __name__ == "__main__":
    main()
