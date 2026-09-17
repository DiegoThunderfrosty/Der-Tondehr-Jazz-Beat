#pragma once

#include <cassert>

#include "HIIR/FPUDownsampler2x.h"
#include "HIIR/FPUUpsampler2x.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace dtjb::dsp {

constexpr double kPi = 3.1415926535897932384626433832795;

inline double clamp(double x, double lo, double hi) noexcept
{
  return std::max(lo, std::min(x, hi));
}

inline double dbToGain(double db) noexcept
{
  return std::pow(10.0, db / 20.0);
}

class SmoothedValue
{
public:
  void prepare(double sampleRate, double timeMs, double initial = 0.0) noexcept
  {
    mCoefficient = std::exp(-1.0 / std::max(1.0, sampleRate * timeMs * 0.001));
    mCurrent = mTarget = initial;
  }

  void setTarget(double target) noexcept { mTarget = target; }
  void setCurrentAndTarget(double value) noexcept { mCurrent = mTarget = value; }

  double next() noexcept
  {
    mCurrent = mTarget + mCoefficient * (mCurrent - mTarget);
    return mCurrent;
  }

  double current() const noexcept { return mCurrent; }

private:
  double mCurrent = 0.0;
  double mTarget = 0.0;
  double mCoefficient = 0.0;
};

class OnePoleLowPass
{
public:
  void prepare(double sampleRate, double cutoffHz) noexcept { setCutoff(sampleRate, cutoffHz); reset(); }
  void setCutoff(double sampleRate, double cutoffHz) noexcept
  {
    const double fc = clamp(cutoffHz, 1.0, sampleRate * 0.45);
    mA = 1.0 - std::exp(-2.0 * kPi * fc / sampleRate);
  }
  void reset(double value = 0.0) noexcept { mState = value; }
  double process(double x) noexcept { mState += mA * (x - mState); return mState; }
private:
  double mA = 1.0;
  double mState = 0.0;
};

class OnePoleHighPass
{
public:
  void prepare(double sampleRate, double cutoffHz) noexcept { mLow.prepare(sampleRate, cutoffHz); }
  void setCutoff(double sampleRate, double cutoffHz) noexcept { mLow.setCutoff(sampleRate, cutoffHz); }
  void reset() noexcept { mLow.reset(); }
  double process(double x) noexcept { return x - mLow.process(x); }
private:
  OnePoleLowPass mLow;
};

class Biquad
{
public:
  void reset() noexcept { mZ1 = mZ2 = 0.0; }

  void setLowPass(double sampleRate, double frequency, double q = 0.70710678) noexcept
  {
    const double w0 = 2.0 * kPi * clamp(frequency, 2.0, sampleRate * 0.45) / sampleRate;
    const double c = std::cos(w0), s = std::sin(w0), alpha = s / (2.0 * q);
    set((1.0 - c) * 0.5, 1.0 - c, (1.0 - c) * 0.5, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
  }

  void setHighPass(double sampleRate, double frequency, double q = 0.70710678) noexcept
  {
    const double w0 = 2.0 * kPi * clamp(frequency, 2.0, sampleRate * 0.45) / sampleRate;
    const double c = std::cos(w0), s = std::sin(w0), alpha = s / (2.0 * q);
    set((1.0 + c) * 0.5, -(1.0 + c), (1.0 + c) * 0.5, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
  }

  void setPeak(double sampleRate, double frequency, double q, double gainDb) noexcept
  {
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * kPi * clamp(frequency, 2.0, sampleRate * 0.45) / sampleRate;
    const double c = std::cos(w0), s = std::sin(w0), alpha = s / (2.0 * q);
    set(1.0 + alpha * A, -2.0 * c, 1.0 - alpha * A,
        1.0 + alpha / A, -2.0 * c, 1.0 - alpha / A);
  }

  void setHighShelf(double sampleRate, double frequency, double gainDb) noexcept
  {
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * kPi * clamp(frequency, 2.0, sampleRate * 0.45) / sampleRate;
    const double c = std::cos(w0), s = std::sin(w0);
    const double alpha = s / std::sqrt(2.0);
    const double twoSqrtAAlpha = 2.0 * std::sqrt(A) * alpha;
    set(A * ((A + 1.0) + (A - 1.0) * c + twoSqrtAAlpha),
        -2.0 * A * ((A - 1.0) + (A + 1.0) * c),
        A * ((A + 1.0) + (A - 1.0) * c - twoSqrtAAlpha),
        (A + 1.0) - (A - 1.0) * c + twoSqrtAAlpha,
        2.0 * ((A - 1.0) - (A + 1.0) * c),
        (A + 1.0) - (A - 1.0) * c - twoSqrtAAlpha);
  }

  double process(double x) noexcept
  {
    const double y = mB0 * x + mZ1;
    mZ1 = mB1 * x - mA1 * y + mZ2;
    mZ2 = mB2 * x - mA2 * y;
    return y;
  }

private:
  void set(double b0, double b1, double b2, double a0, double a1, double a2) noexcept
  {
    const double invA0 = 1.0 / a0;
    mB0 = b0 * invA0; mB1 = b1 * invA0; mB2 = b2 * invA0;
    mA1 = a1 * invA0; mA2 = a2 * invA0;
  }

  double mB0 = 1.0, mB1 = 0.0, mB2 = 0.0, mA1 = 0.0, mA2 = 0.0;
  double mZ1 = 0.0, mZ2 = 0.0;
};

class FirstOrderAllPass
{
public:
  void setCoefficient(double coefficient) noexcept { mA = clamp(coefficient, -0.98, 0.98); }
  void reset() noexcept { mX1 = mY1 = 0.0; }
  double process(double x) noexcept
  {
    const double y = -mA * x + mX1 + mA * mY1;
    mX1 = x; mY1 = y;
    return y;
  }
private:
  double mA = 0.5, mX1 = 0.0, mY1 = 0.0;
};

template <std::size_t Size>
class FractionalDelay
{
public:
  void reset() noexcept { mBuffer.fill(0.0f); mWrite = 0; }

  double process(double input, double delaySamples) noexcept
  {
    mBuffer[mWrite] = static_cast<float>(input);
    double read = static_cast<double>(mWrite) - clamp(delaySamples, 2.0, static_cast<double>(Size - 3));
    while(read < 0.0) read += static_cast<double>(Size);
    const auto i0 = static_cast<std::size_t>(read) % Size;
    const auto i1 = (i0 + 1) % Size;
    const double frac = read - std::floor(read);
    const double output = static_cast<double>(mBuffer[i0]) +
                          frac * (static_cast<double>(mBuffer[i1]) - static_cast<double>(mBuffer[i0]));
    mWrite = (mWrite + 1) % Size;
    return output;
  }

private:
  std::array<float, Size> mBuffer{};
  std::size_t mWrite = 0;
};

class TriangleLFO
{
public:
  void prepare(double sampleRate) noexcept { mSampleRate = sampleRate; mPhase = 0.0; }
  void reset() noexcept { mPhase = 0.0; }
  double process(double frequencyHz) noexcept
  {
    mPhase += frequencyHz / mSampleRate;
    if(mPhase >= 1.0) mPhase -= 1.0;
    return 1.0 - 4.0 * std::abs(mPhase - 0.5);
  }
private:
  double mSampleRate = 48000.0;
  double mPhase = 0.0;
};

class LocalOversampler
{
public:
  LocalOversampler() noexcept
  {
    // The same proven polyphase all-pass coefficients used by iPlug2's own
    // oversampler. The first half-band stage is intentionally steeper because
    // it is also the final anti-alias stage on the way back to the host rate.
    static constexpr double kFirstStage[12] = {
      0.036681502163648017, 0.13654762463195794, 0.27463175937945444,
      0.42313861743656711, 0.56109869787919531, 0.67754004997416184,
      0.76974183386322703, 0.83988962484963892, 0.89226081800387902,
      0.9315419599631839, 0.96209454837808417, 0.98781637073289585
    };
    static constexpr double kSecondStage[4] = {
      0.041893991997656171, 0.16890348243995201,
      0.39056077292116603, 0.74389574826847926
    };
    mUp2x.set_coefs(kFirstStage);
    mDown2x.set_coefs(kFirstStage);
    mUp4x.set_coefs(kSecondStage);
    mDown4x.set_coefs(kSecondStage);
    reset();
  }

  void reset() noexcept
  {
    clearFilterMemory();
    mActiveFactor = 0;
  }

  template <typename Nonlinearity>
  double process(double x, int factor, Nonlinearity&& nonlinearity) noexcept
  {
    factor = factor <= 1 ? 1 : (factor <= 2 ? 2 : 4);
    if(factor != mActiveFactor)
    {
      clearFilterMemory();
      mActiveFactor = factor;
    }

    if(factor == 1)
      return nonlinearity(x);

    double up2x[2]{};
    mUp2x.process_sample(up2x[0], up2x[1], x);

    if(factor == 2)
    {
      const double processed[2] = {
        nonlinearity(up2x[0]), nonlinearity(up2x[1])
      };
      return mDown2x.process_sample(processed);
    }

    double up4x[4]{};
    double down2x[2]{};
    mUp4x.process_block(up4x, up2x, 2);
    for(double& sample : up4x)
      sample = nonlinearity(sample);
    mDown4x.process_block(down2x, up4x, 2);
    return mDown2x.process_sample(down2x);
  }

private:
  void clearFilterMemory() noexcept
  {
    mUp2x.clear_buffers();
    mDown2x.clear_buffers();
    mUp4x.clear_buffers();
    mDown4x.clear_buffers();
  }

  int mActiveFactor = 0;
  hiir::Upsampler2xFPU<12, double> mUp2x;
  hiir::Downsampler2xFPU<12, double> mDown2x;
  hiir::Upsampler2xFPU<4, double> mUp4x;
  hiir::Downsampler2xFPU<4, double> mDown4x;
};

} // namespace dtjb::dsp
