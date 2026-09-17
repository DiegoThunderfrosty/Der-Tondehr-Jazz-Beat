#pragma once

#include "DspCommon.h"

namespace dtjb::dsp {

class BBDChorus
{
public:
  enum class Mode : int { Off = 0, Fixed = 1, Manual = 2 };

  void prepare(double sampleRate)
  {
    mSampleRate = sampleRate;
    // C58 47 nF with R109 + R108 (5.6 k + 56 k): ~55 Hz.
    mInputHighPass.setHighPass(sampleRate, 55.0, 0.707);
    // R109/C64 and R108/C65 both land close to 12.9 kHz.
    mPreLowPass1.setLowPass(sampleRate, 12920.0, 0.68);
    mPreLowPass2.setLowPass(sampleRate, 12920.0, 0.68);
    // MN3007 recovery ladder: 8.2 k sections with 2.2 nF, 8.2 nF and 470 pF.
    mPostLowPass1.setLowPass(sampleRate, 8820.0, 0.72);
    mPostLowPass2.setLowPass(sampleRate, 2370.0, 0.62);
    mDeEmphasis.setLowPass(sampleRate, 720.0, 0.55); // R107/C70 active de-emphasis branch.
    mLFO.prepare(sampleRate);
    mRate.prepare(sampleRate, 30.0, 1.6);
    mDepth.prepare(sampleRate, 30.0, 0.62);
    reset();
  }

  void reset() noexcept
  {
    mDelay.reset();
    mInputHighPass.reset(); mPreLowPass1.reset(); mPreLowPass2.reset();
    mPostLowPass1.reset(); mPostLowPass2.reset(); mDeEmphasis.reset();
    mLFO.reset();
    mNoiseState = 0x1234abcdU;
  }

  void setMode(Mode mode) noexcept { mMode = mode; }
  void setRate(double normalized) noexcept
  {
    // Service drawing: 240 ms to 1 s triangle period (~1 to 4.17 Hz).
    mRate.setTarget(1.0 + 3.17 * clamp(normalized, 0.0, 1.0));
  }
  void setDepth(double normalized) noexcept { mDepth.setTarget(clamp(normalized, 0.0, 1.0)); }

  bool enabled() const noexcept { return mMode != Mode::Off; }

  double process(double x) noexcept
  {
    if(mMode == Mode::Off)
      return 0.0;

    double encoded = mInputHighPass.process(x);
    encoded = mPreLowPass2.process(mPreLowPass1.process(encoded));
    // A lightweight companding proxy at the BBD input only, not a global waveshaper.
    encoded = 0.78 * std::tanh(encoded / 0.78);

    const double rateHz = (mMode == Mode::Fixed) ? 1.18 : mRate.next();
    const double depth = (mMode == Mode::Fixed) ? 0.58 : mDepth.next();
    const double triangle = mLFO.process(rateHz);

    // MN3101 clock range shown in the drawing: 80-350 kHz. MN3007 delay is
    // approximately N/(2*fclock), N=1024 stages.
    const double centerClock = (mMode == Mode::Fixed) ? 188000.0 : 215000.0;
    const double clockHz = clamp(centerClock * (1.0 + triangle * 0.47 * depth), 80000.0, 350000.0);
    const double delaySamples = mSampleRate * 1024.0 / (2.0 * clockHz);

    // Finite BBD charge resolution and deterministic low-level clock/hash noise.
    const double quantized = std::round(encoded * 4096.0) / 4096.0;
    mNoiseState = mNoiseState * 1664525U + 1013904223U;
    const double noise = (static_cast<double>((mNoiseState >> 9) & 0x7fffffU) / 4194304.0 - 1.0) * 1.2e-5;
    double delayed = mDelay.process(quantized + noise, delaySamples);

    delayed = mPostLowPass1.process(delayed);
    const double dark = mPostLowPass2.process(delayed);
    const double deemphasized = mDeEmphasis.process(delayed);
    // Preserve some upper-mid energy while honoring the multi-pole BBD recovery filter.
    return 0.48 * delayed + 0.34 * dark + 0.18 * deemphasized;
  }

private:
  static constexpr std::size_t kBBDDelayCapacity = 8192;
  double mSampleRate = 48000.0;
  Mode mMode = Mode::Off;
  FractionalDelay<kBBDDelayCapacity> mDelay;
  Biquad mInputHighPass, mPreLowPass1, mPreLowPass2;
  Biquad mPostLowPass1, mPostLowPass2, mDeEmphasis;
  TriangleLFO mLFO;
  SmoothedValue mRate, mDepth;
  std::uint32_t mNoiseState = 0x1234abcdU;
};

} // namespace dtjb::dsp

