#pragma once

#include "DspCommon.h"

namespace dtjb::dsp {

class DistortionStage
{
public:
  void prepare(double sampleRate)
  {
    mSampleRate = sampleRate;
    // C30 82 nF and the two 3.9 kOhm series resistors establish the
    // frequency-dependent drive into the diode/pot network (~249 Hz).
    mDriveHighPass.prepare(sampleRate, 1.0 / (2.0 * kPi * 7800.0 * 82.0e-9));
    mPostLowPass.setLowPass(sampleRate, 11800.0, 0.65);
    mAmount.prepare(sampleRate, 15.0, 0.0);
    reset();
  }

  void reset() noexcept
  {
    mDriveHighPass.reset();
    mPostLowPass.reset();
    mOversampler.reset();
  }

  void setEnabled(bool enabled) noexcept { mEnabled = enabled; }
  void setAmount(double normalized) noexcept { mAmount.setTarget(clamp(normalized, 0.0, 1.0)); }
  void setOversampling(int factor) noexcept
  {
    mOversampling = factor <= 1 ? 1 : (factor <= 2 ? 2 : 4);
  }

  double process(double x) noexcept
  {
    if(!mEnabled)
      return x;

    const double amount = mAmount.next();
    const double shaped = 0.55 * x + 0.45 * mDriveHighPass.process(x);
    const double drive = 1.0 + 34.0 * amount * amount;
    const double thresholdPositive = 0.52 - 0.20 * amount;
    const double thresholdNegative = 0.58 - 0.18 * amount;

    const double clipped = mOversampler.process(shaped * drive, mOversampling,
      [thresholdPositive, thresholdNegative](double v) noexcept
      {
        const double threshold = v >= 0.0 ? thresholdPositive : thresholdNegative;
        // D1/D9 and D10/D8 are represented as two slightly mismatched
        // antiparallel silicon branches at the exact stage where they occur.
        return threshold * std::tanh(v / std::max(0.05, threshold));
      });

    const double compensated = clipped / std::sqrt(drive);
    return mPostLowPass.process((1.0 - 0.12 * amount) * compensated + 0.08 * x);
  }

private:
  double mSampleRate = 48000.0;
  bool mEnabled = false;
  int mOversampling = 2;
  OnePoleHighPass mDriveHighPass;
  Biquad mPostLowPass;
  SmoothedValue mAmount;
  LocalOversampler mOversampler;
};

} // namespace dtjb::dsp
