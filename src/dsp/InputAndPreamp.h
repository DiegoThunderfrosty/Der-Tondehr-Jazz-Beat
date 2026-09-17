#pragma once

#include "DspCommon.h"

namespace dtjb::dsp {

class InputStage
{
public:
  void prepare(double sampleRate)
  {
    mSampleRate = sampleRate;
    // C12 82 nF into R16 39 kOhm: 49.8 Hz.
    mCoupling.prepare(sampleRate, 1.0 / (2.0 * kPi * 39000.0 * 82.0e-9));
    mTrim.prepare(sampleRate, 20.0, 1.0);
  }

  void reset() noexcept { mCoupling.reset(); }
  void setInputTrimDb(double db) noexcept { mTrim.setTarget(dbToGain(db)); }
  void setLowInput(bool low) noexcept { mLowInput = low; }

  double process(double x) noexcept
  {
    // The service drawing specifies 21 mVrms at HIGH and 85 mVrms at LOW
    // for the same downstream reference level: 21/85 = 0.247.
    const double jackGain = mLowInput ? (21.0 / 85.0) : 1.0;
    return mCoupling.process(x * jackGain * mTrim.next());
  }

private:
  double mSampleRate = 48000.0;
  bool mLowInput = false;
  OnePoleHighPass mCoupling;
  SmoothedValue mTrim;
};

class PreampStage
{
public:
  void prepare(double sampleRate)
  {
    // R18/C20 and R23/C24 both give approximately 33.9 kHz.
    mStage1Bandwidth.setLowPass(sampleRate, 33862.0, 0.707);
    mStage2Bandwidth.setLowPass(sampleRate, 33862.0, 0.707);
    reset();
  }

  void reset() noexcept
  {
    mStage1Bandwidth.reset();
    mStage2Bandwidth.reset();
    mStage1Oversampler.reset();
    mStage2Oversampler.reset();
  }

  void setOversampling(int factor) noexcept
  {
    mOversampling = factor <= 1 ? 1 : (factor <= 2 ? 2 : 4);
  }

  double process(double x) noexcept
  {
    // Measured service points: 21 mV -> 72 mV -> 130 mV. The two tanh calls
    // are the local nonlinear devices in this stage, so the user's selected
    // factor is honored here continuously. No surrounding tone/filter/reverb
    // processing is moved to the oversampled rate.
    double y = mStage1Bandwidth.process(x * (72.0 / 21.0));
    y = mStage1Oversampler.process(y, mOversampling,
      [](double v) noexcept { return 2.8 * std::tanh(v / 2.8); });

    y = mStage2Bandwidth.process(y * (130.0 / 72.0));
    return mStage2Oversampler.process(y, mOversampling,
      [](double v) noexcept { return 3.2 * std::tanh(v / 3.2); });
  }

private:
  int mOversampling = 2;
  Biquad mStage1Bandwidth;
  Biquad mStage2Bandwidth;
  LocalOversampler mStage1Oversampler;
  LocalOversampler mStage2Oversampler;
};

} // namespace dtjb::dsp
