#pragma once

// Blinker brightness curve, sampled from the real PWM fade played by
// the kernel LED module on the vehicle (fade10-blink). Keeping this in
// lockstep with the hardware means the UI arrow fades with the same
// shape and duration as the physical turn-signal lamp.
//
// The kernel module advances one sample per 1/sample_rate second.
// imx_pwm_led.conf sets sample_rate=250 -> 4 ms per sample. The
// blinker goroutine in vehicle-service restarts the cue every
// blinkerInterval=800 ms, so the 126-sample (504 ms) active portion
// is followed by 296 ms of dark before the next cycle.
//
// Values are normalised to the peak duty of the original fade.

namespace blinker {

constexpr int kSampleCount = 126;
constexpr int kSampleMs = 4;
constexpr int kActiveMs = kSampleCount * kSampleMs; // 504
constexpr int kCycleMs = 800;                       // matches vehicle-service blinkerInterval

constexpr float kCurve[kSampleCount] = {
    0.000000f, 0.032147f, 0.064295f, 0.096442f, 0.128589f, 0.160737f, 0.192884f, 0.225031f,
    0.257179f, 0.289242f, 0.321473f, 0.353621f, 0.385768f, 0.417915f, 0.450063f, 0.482210f,
    0.514357f, 0.546505f, 0.578568f, 0.604939f, 0.614232f, 0.623441f, 0.632650f, 0.641775f,
    0.650984f, 0.660193f, 0.669318f, 0.678527f, 0.687735f, 0.696944f, 0.706069f, 0.715278f,
    0.724487f, 0.733612f, 0.742821f, 0.752030f, 0.761239f, 0.770364f, 0.779573f, 0.788782f,
    0.797907f, 0.807116f, 0.816325f, 0.825534f, 0.834659f, 0.843868f, 0.853077f, 0.862202f,
    0.871411f, 0.880620f, 0.889828f, 0.898954f, 0.908162f, 0.917371f, 0.926496f, 0.935705f,
    0.944914f, 0.954123f, 0.963248f, 0.972457f, 0.981666f, 0.990791f, 1.000000f, 0.995312f,
    0.990791f, 0.981666f, 0.972457f, 0.963248f, 0.954123f, 0.944914f, 0.935705f, 0.926496f,
    0.917371f, 0.908162f, 0.898954f, 0.889828f, 0.880620f, 0.871411f, 0.862202f, 0.853077f,
    0.843868f, 0.834659f, 0.825534f, 0.816325f, 0.807116f, 0.797907f, 0.788782f, 0.779573f,
    0.770364f, 0.761239f, 0.752030f, 0.742821f, 0.733612f, 0.724487f, 0.715278f, 0.706069f,
    0.696944f, 0.687735f, 0.678527f, 0.669318f, 0.660193f, 0.650984f, 0.641775f, 0.632650f,
    0.623441f, 0.614232f, 0.604939f, 0.578568f, 0.546505f, 0.514357f, 0.482210f, 0.450063f,
    0.417915f, 0.385768f, 0.353621f, 0.321473f, 0.289242f, 0.257179f, 0.225031f, 0.192884f,
    0.160737f, 0.128589f, 0.096442f, 0.064295f, 0.032147f, 0.000000f
};

// Sample the curve at the given phase within the 800 ms cycle, with
// linear interpolation between samples. Returns 0 for phases past the
// active window.
inline float sample(int phaseMs)
{
    if (phaseMs < 0) phaseMs = ((phaseMs % kCycleMs) + kCycleMs) % kCycleMs;
    else if (phaseMs >= kCycleMs) phaseMs %= kCycleMs;

    if (phaseMs >= kActiveMs) return 0.0f;

    const float pos = phaseMs / float(kSampleMs);
    const int i = int(pos);
    const float frac = pos - i;
    const int j = (i + 1 < kSampleCount) ? (i + 1) : (kSampleCount - 1);
    return kCurve[i] * (1.0f - frac) + kCurve[j] * frac;
}

} // namespace blinker
