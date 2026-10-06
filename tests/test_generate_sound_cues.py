import importlib.util
import struct
import unittest
import wave
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location(
    "sound_cues", ROOT / "scripts" / "generate-sound-cues.py"
)
cues = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cues)


def curvature(samples):
    return max(abs(c - 2 * b + a) for a, b, c in zip(samples, samples[1:], samples[2:]))


class SoundCueGenerationTest(unittest.TestCase):
    def test_chimes_end_at_zero(self):
        for frequency, duration in [(260, 0.32), (390, 0.46), (493.88, 0.56)]:
            with self.subTest(frequency=frequency):
                samples = cues.add_chime([], 0, frequency, duration, 0.9)
                self.assertEqual(samples[0], 0)
                self.assertAlmostEqual(samples[-1], 0, places=12)
                self.assertLess(curvature(samples), 0.003)

    def test_unlock_level_transitions_are_smooth(self):
        rendered = {}
        with patch.object(
            cues, "write_stereo",
            side_effect=lambda name, samples: rendered.update({name: samples}),
        ):
            cues.main()
        self.assertLess(curvature(rendered["scooter-unlock.wav"]), 0.002)

    def test_packaged_waveforms_have_no_clicks_or_clipping(self):
        for path in sorted((ROOT / "assets" / "sounds").glob("*.wav")):
            with self.subTest(cue=path.name), wave.open(str(path)) as audio:
                self.assertEqual(audio.getframerate(), 48000)
                self.assertEqual(audio.getnchannels(), 1)
                self.assertEqual(audio.getsampwidth(), 2)
                data = audio.readframes(audio.getnframes())
                samples = [value / 32768 for (value,) in struct.iter_unpack("<h", data)]
                self.assertEqual(samples[0], 0)
                self.assertEqual(samples[-1], 0)
                self.assertLessEqual(max(map(abs, samples)), 10 ** (-1.5 / 20) + 1 / 32768)
                self.assertLess(curvature(samples), 0.012)


if __name__ == "__main__":
    unittest.main()
