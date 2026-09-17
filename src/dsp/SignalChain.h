#pragma once

#include "BBDChorus.h"
#include "DistortionStage.h"
#include "InputAndPreamp.h"
#include "PowerAmp.h"
#include "SpringReverb.h"
#include "ToneAndVolume.h"

namespace dtjb::dsp {

struct Parameters
{
  bool lowInput = false;
  double inputTrimDb = 0.0;
  bool distortionEnabled = false;
  double distortion = 0.0;
  double bass = 0.5;
  double middle = 0.5;
  double treble = 0.5;
  double hiTreble = 0.0;
  double volume = 0.55;
  double reverb = 0.18;
  BBDChorus::Mode chorusMode = BBDChorus::Mode::Off;
  double chorusRate = 0.35;
  double chorusDepth = 0.62;
  int oversampling = 2;
  double outputTrimDb = 0.0;
};

struct StereoSample
{
  double left = 0.0;
  double right = 0.0;
};

class SignalChain
{
public:
  void prepare(double sampleRate)
  {
    mSampleRate = sampleRate;
    mInput.prepare(sampleRate);
    mPreamp.prepare(sampleRate);
    mDistortion.prepare(sampleRate);
    mTone.prepare(sampleRate);
    mVolume.prepare(sampleRate);
    mReverb.prepare(sampleRate);
    mChorus.prepare(sampleRate);
    mPowerLeft.prepare(sampleRate, -0.008);
    mPowerRight.prepare(sampleRate, 0.011);
    mOutputGain.prepare(sampleRate, 20.0, dbToGain(mParameters.outputTrimDb));
    reset();
    setParameters(mParameters);
  }

  void reset() noexcept
  {
    mInput.reset(); mPreamp.reset(); mDistortion.reset(); mTone.reset();
    mVolume.reset(); mReverb.reset(); mChorus.reset();
    mPowerLeft.reset(); mPowerRight.reset();
    mOutputGain.setCurrentAndTarget(dbToGain(clamp(mParameters.outputTrimDb, -24.0, 12.0)));
  }

  void setParameters(const Parameters& parameters) noexcept
  {
    mParameters = parameters;
    mInput.setLowInput(parameters.lowInput);
    mInput.setInputTrimDb(parameters.inputTrimDb);
    mDistortion.setEnabled(parameters.distortionEnabled);
    mDistortion.setAmount(parameters.distortion);
    mDistortion.setOversampling(parameters.oversampling);
    mPreamp.setOversampling(parameters.oversampling);
    mTone.setControls(parameters.bass,
                      parameters.middle,
                      parameters.treble,
                      parameters.hiTreble,
                      parameters.volume);
    mVolume.setLevel(parameters.volume);
    mReverb.setLevel(parameters.reverb);
    mChorus.setMode(parameters.chorusMode);
    mChorus.setRate(parameters.chorusRate);
    mChorus.setDepth(parameters.chorusDepth);
    mPowerLeft.setOversampling(parameters.oversampling);
    mPowerRight.setOversampling(parameters.oversampling);
    mOutputGain.setTarget(dbToGain(clamp(parameters.outputTrimDb, -24.0, 12.0)));
  }

  StereoSample process(double inputLeft, double inputRight) noexcept
  {
    // The physical amplifier has a mono front end. Stereo hosts are folded
    // before the HIGH/LOW input and expanded only by the original chorus route.
    const double monoInput = 0.5 * (inputLeft + inputRight);
    double signal = mInput.process(monoInput);
    signal = mPreamp.process(signal);
    signal = mDistortion.process(signal);
    signal = mTone.process(signal);
    const double dry = mVolume.process(signal);

    const double recoveredSpring = mReverb.process(dry);
    const double mixed = dry + recoveredSpring;
    const double bbd = mChorus.process(mixed);

    // Opposed BBD contribution reproduces the two-amplifier spatial routing.
    const double leftDrive = mixed + (mChorus.enabled() ? 0.72 * bbd : 0.0);
    const double rightDrive = mixed - (mChorus.enabled() ? 0.72 * bbd : 0.0);
    const double outputGain = mOutputGain.next();
    return {mPowerLeft.process(leftDrive) * outputGain,
            mPowerRight.process(rightDrive) * outputGain};
  }

private:
  double mSampleRate = 48000.0;
  Parameters mParameters{};
  InputStage mInput;
  PreampStage mPreamp;
  DistortionStage mDistortion;
  ToneNetworkStage mTone;
  VolumeStage mVolume;
  SpringReverb mReverb;
  BBDChorus mChorus;
  PowerAmplifier mPowerLeft, mPowerRight;
  SmoothedValue mOutputGain;
};

} // namespace dtjb::dsp
