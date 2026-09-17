#pragma once

#include "DspCommon.h"

namespace dtjb::dsp {

class PowerAmplifier
{
public:
  void prepare(double sampleRate, double channelTolerance)
  {
    mSampleRate = sampleRate;
    mTolerance = channelTolerance;
    // C21/C60 82 nF into R36/R105 56 kOhm: 34.7 Hz.
    mInputCoupling.setHighPass(sampleRate, 1.0 / (2.0 * kPi * 56000.0 * 82.0e-9), 0.707);
    mDriverBandwidth.setLowPass(sampleRate, 42000.0 * (1.0 + channelTolerance), 0.68);
    mSagDetector.prepare(sampleRate, 7.0);
    mEnvelopeAttack = 1.0 - std::exp(-1.0 / std::max(1.0, sampleRate * 0.0005));
    mEnvelopeRelease = 1.0 - std::exp(-1.0 / std::max(1.0, sampleRate * 0.040));
    // R77/C48 (and R149/C85) are 2.7 Ohm / 0.01 uF Zobels: their ideal
    // corner is ~5.89 MHz, so they do not impose an audible 5.9 kHz low-pass.
    mOutputStability.setLowPass(sampleRate, std::min(100000.0, sampleRate * 0.44), 0.68);
    reset();
  }

  void reset() noexcept
  {
    mInputCoupling.reset(); mDriverBandwidth.reset(); mSagDetector.reset();
    mOutputStability.reset(); mOversampler.reset();
    mLastOutputVolts = 0.0;
    mDriveEnvelope = 0.0;
  }

  void setOversampling(int factor) noexcept
  {
    mOversampling = factor <= 1 ? 1 : (factor <= 2 ? 2 : 4);
  }

  double process(double inputVolts) noexcept
  {
    constexpr double kNominalLoadOhms = 8.0;
    constexpr double kSupplyRailVolts = 33.5;
    constexpr double kReferencePeakVolts = 22.0 * 1.4142135623730951;

    // Service voltages imply about 27.5 V/V from the right amplifier's
    // 800 mVrms test point to 22 Vrms at the 8 Ohm load.
    const double smallSignal = mDriverBandwidth.process(mInputCoupling.process(inputVolts)) *
                               27.5 * (1.0 + mTolerance);

    const double loadCurrent = std::abs(mLastOutputVolts) / kNominalLoadOhms;
    const double currentEnvelope = mSagDetector.process(loadCurrent);
    const double availableRail = std::max(27.0, kSupplyRailVolts - 0.48 * currentEnvelope);

    // Spend the extra nonlinear evaluations only as the output stage
    // approaches compression. A fast-attack/slow-release envelope prevents
    // the stateful half-band filters from changing rate at every waveform
    // zero crossing.
    const double driveMagnitude = std::abs(smallSignal);
    const double envelopeCoefficient = driveMagnitude > mDriveEnvelope
      ? mEnvelopeAttack
      : mEnvelopeRelease;
    mDriveEnvelope += envelopeCoefficient * (driveMagnitude - mDriveEnvelope);
    const int nonlinearFactor = mDriveEnvelope > availableRail * 0.55
      ? mOversampling
      : 1;
    const double outputVolts = mOversampler.process(smallSignal, nonlinearFactor,
      [availableRail](double v) noexcept
      {
        // Reduced complementary output-stage model: a small crossover region
        // followed by rail/current compression. No per-sample SPICE solver.
        const double magnitude = std::abs(v);
        double crossover = magnitude < 0.045 ? v * 0.74 : std::copysign(magnitude - 0.0117, v);
        return availableRail * std::tanh(crossover / availableRail);
      });

    mLastOutputVolts = mOutputStability.process(outputVolts);
    return clamp(mLastOutputVolts / kReferencePeakVolts, -1.12, 1.12);
  }

private:
  double mSampleRate = 48000.0;
  double mTolerance = 0.0;
  double mLastOutputVolts = 0.0;
  double mDriveEnvelope = 0.0;
  double mEnvelopeAttack = 1.0;
  double mEnvelopeRelease = 1.0;
  int mOversampling = 2;
  Biquad mInputCoupling, mDriverBandwidth, mOutputStability;
  OnePoleLowPass mSagDetector;
  LocalOversampler mOversampler;
};

} // namespace dtjb::dsp
