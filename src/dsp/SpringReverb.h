#pragma once

#include "DspCommon.h"

namespace dtjb::dsp {

class ReverbDriverStage
{
public:
  void prepare(double sampleRate)
  {
    mSampleRate = sampleRate;
    mLowCut.setHighPass(sampleRate, 175.0, 0.707);
    // 0.8 Ohm DCR + approximately 3.3 Ohm driver resistance with the
    // estimated 1.59 mH send inductance gives roughly 410 Hz.
    mSendRL.prepare(sampleRate, (0.8 + 3.3) / (2.0 * kPi * 1.59e-3));
    mUpperBand.setLowPass(sampleRate, 4800.0, 0.65);
    reset();
  }

  void reset() noexcept { mLowCut.reset(); mSendRL.reset(); mUpperBand.reset(); }

  double process(double x) noexcept
  {
    double y = mLowCut.process(x);
    const double rlCurrent = mSendRL.process(y);
    y = 0.58 * rlCurrent + 0.42 * y;
    y = mUpperBand.process(y);
    // IC1b plus Q3/Q4 emitter followers and their 3.3 Ohm resistors.
    return 1.45 * std::tanh(y / 1.45);
  }

private:
  double mSampleRate = 48000.0;
  Biquad mLowCut, mUpperBand;
  OnePoleLowPass mSendRL;
};

class SpringPath
{
public:
  void prepare(double sampleRate, double delayMs, double decaySeconds,
               double dampingHz, double modal1Hz, double modal2Hz,
               const std::array<double, 4>& dispersion) noexcept
  {
    mSampleRate = sampleRate;
    mDelaySamples = delayMs * 0.001 * sampleRate;
    mFeedback = std::pow(0.001, (delayMs * 0.001) / decaySeconds);
    mLowCut.setHighPass(sampleRate, 175.0 + 0.7 * delayMs, 0.65);
    mDamping.setLowPass(sampleRate, dampingHz, 0.65);
    mMode1.setPeak(sampleRate, modal1Hz, 7.0, 6.0);
    mMode2.setPeak(sampleRate, modal2Hz, 9.0, 4.5);
    for(std::size_t i = 0; i < mDispersion.size(); ++i)
      mDispersion[i].setCoefficient(dispersion[i]);
    reset();
  }

  void reset() noexcept
  {
    mDelay.reset();
    mLowCut.reset(); mDamping.reset(); mMode1.reset(); mMode2.reset();
    for(auto& stage : mDispersion) stage.reset();
    mFeedbackState = 0.0;
  }

  double process(double input, double delayModulationSamples) noexcept
  {
    double y = mDelay.process(input + mFeedbackState * mFeedback,
                              mDelaySamples + delayModulationSamples);
    for(auto& stage : mDispersion)
      y = stage.process(y);
    y = mDamping.process(mLowCut.process(y));
    const double resonant = 0.58 * y + 0.25 * mMode1.process(y) + 0.17 * mMode2.process(y);
    mFeedbackState = clamp(0.78 * y + 0.22 * resonant, -4.0, 4.0);
    return resonant;
  }

private:
  static constexpr std::size_t kDelayCapacity = 32768;
  double mSampleRate = 48000.0;
  double mDelaySamples = 1800.0;
  double mFeedback = 0.85;
  double mFeedbackState = 0.0;
  FractionalDelay<kDelayCapacity> mDelay;
  std::array<FirstOrderAllPass, 4> mDispersion{};
  Biquad mLowCut, mDamping, mMode1, mMode2;
};

class ThreeSpringTank
{
public:
  void prepare(double sampleRate)
  {
    mSampleRate = sampleRate;
    // Non-identical travel times, damping, modal frequencies and dispersion.
    // The feedback coefficients produce approximately 2.25 s T60.
    mSprings[0].prepare(sampleRate, 31.7, 2.20, 4300.0, 1080.0, 2360.0,
                        {0.31, 0.47, 0.58, 0.69});
    mSprings[1].prepare(sampleRate, 38.9, 2.36, 3900.0, 1370.0, 2810.0,
                        {0.35, 0.52, 0.63, 0.73});
    mSprings[2].prepare(sampleRate, 46.3, 2.52, 3500.0, 1710.0, 3190.0,
                        {0.39, 0.56, 0.67, 0.77});
    mMotion.prepare(sampleRate);
    mOutputLowPass.setLowPass(sampleRate, 5000.0, 0.62);
    reset();
  }

  void reset() noexcept
  {
    for(auto& spring : mSprings) spring.reset();
    mMotion.reset();
    mOutputLowPass.reset();
  }

  double process(double input) noexcept
  {
    const double slowMotion = mMotion.process(0.17);
    const double a = mSprings[0].process(input * 0.39, slowMotion * 0.31);
    const double b = mSprings[1].process(input * 0.34, -slowMotion * 0.43);
    const double c = mSprings[2].process(input * 0.27, slowMotion * 0.57);
    // Small unequal early-reflection combination produces the characteristic
    // splash without a generic allpass/comb reverb topology.
    return mOutputLowPass.process(0.93 * a - 0.72 * b + 0.81 * c);
  }

private:
  double mSampleRate = 48000.0;
  std::array<SpringPath, 3> mSprings{};
  TriangleLFO mMotion;
  Biquad mOutputLowPass;
};

class ReverbRecoveryStage
{
public:
  void prepare(double sampleRate)
  {
    mSampleRate = sampleRate;
    // C52 82 nF / R81 15 kOhm = 129 Hz. IC4b gain = 1 + 56k/8.2k.
    mCoupling.setHighPass(sampleRate, 1.0 / (2.0 * kPi * 15000.0 * 82.0e-9), 0.707);
    // R79 56 kOhm / C46 47 pF = 60.5 kHz, limited to a safe Nyquist fraction.
    mOpAmpBandwidth.setLowPass(sampleRate, std::min(60500.0, sampleRate * 0.44), 0.707);
    mReturnTransducer.setLowPass(sampleRate, 4400.0, 0.7);
    reset();
  }

  void reset() noexcept { mCoupling.reset(); mOpAmpBandwidth.reset(); mReturnTransducer.reset(); }

  double process(double x) noexcept
  {
    // Return coil: 200 Ohm DCR, estimated 0.409 H. The following input stage
    // loads it lightly; its dominant audible result is the measured HF loss.
    double y = mReturnTransducer.process(x);
    y = mCoupling.process(y);
    y = mOpAmpBandwidth.process(y * (1.0 + 56000.0 / 8200.0));
    return 2.4 * std::tanh(y / 2.4);
  }

private:
  double mSampleRate = 48000.0;
  Biquad mCoupling, mOpAmpBandwidth, mReturnTransducer;
};

class SpringReverb
{
public:
  void prepare(double sampleRate)
  {
    mDriver.prepare(sampleRate);
    mTank.prepare(sampleRate);
    mRecovery.prepare(sampleRate);
    mLevel.prepare(sampleRate, 25.0, 0.18);
  }

  void reset() noexcept { mDriver.reset(); mTank.reset(); mRecovery.reset(); }
  void setLevel(double normalized) noexcept { mLevel.setTarget(clamp(normalized, 0.0, 1.0)); }

  double process(double send) noexcept
  {
    const double tankInput = mDriver.process(send);
    const double tankOutput = mTank.process(tankInput);
    const double recovered = mRecovery.process(tankOutput * 0.22);
    const double level = mLevel.next();
    // VR7 is 10 kOhm B (linear).
    return recovered * level;
  }

private:
  ReverbDriverStage mDriver;
  ThreeSpringTank mTank;
  ReverbRecoveryStage mRecovery;
  SmoothedValue mLevel;
};

} // namespace dtjb::dsp

