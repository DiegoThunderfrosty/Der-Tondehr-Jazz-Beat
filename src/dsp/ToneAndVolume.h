#pragma once

#include "DspCommon.h"

#include <array>
#include <cmath>

namespace dtjb::dsp {

/**
 * Passive tone + volume network reconstructed from the service schematic.
 *
 * Schematic values used here:
 *   R74  = 100 kOhm tone-slope resistor
 *   C49  = 150 pF treble capacitor
 *   C50  = 82 nF bass capacitor
 *   C51  = 47 nF middle capacitor
 *   VR4  = 250 kOhm B (linear) treble pot
 *   VR6  = 250 kOhm A (audio/log) bass pot, wired as a rheostat
 *   VR5  = 10 kOhm B (linear) middle pot, wired as a rheostat to ground
 *   R97  = 1 kOhm in series with the bass rheostat
 *   VR2  = 1 MOhm B (linear) volume potentiometer
 *   R42  = 100 kOhm fixed feed from the VR4 wiper to the isolated TOP of VR2
 *   C42  = 470 pF in series with VR3
 *   VR3  = 1 MOhm B (linear) HI-TREBLE rheostat from VR4 wiper to VR2 wiper
 *   C37  = 82 nF output coupling capacitor
 *   R47  = 1 MOhm load after C37
 *
 * A critical detail in the schematic is that HI-TREBLE is NOT a shelf placed
 * after the tone stack. R42 (100 kOhm) feeds the TREBLE wiper into the TOP of
 * VR2, while only the C42+VR3 branch bypasses that path and lands directly at
 * the WIPER of VR2. The TOP of VR2 is a separate node from the C51/VR5
 * middle-shunt node; VR2's bottom terminal is grounded. This is what lets Volume
 * remain a genuine level control while the bright/HI-TREBLE branch changes its
 * relative contribution as the Volume setting moves.
 */
class ToneNetworkStage
{
public:
  void prepare(double sampleRate)
  {
    mSampleRate = std::max(8000.0, sampleRate);
    mTrebleCap.prepare(mSampleRate, 150.0e-12);
    mBassCap.prepare(mSampleRate, 82.0e-9);
    mMiddleCap.prepare(mSampleRate, 47.0e-9);
    mHiTrebleCap.prepare(mSampleRate, 470.0e-12);
    mVolumeCouplingCap.prepare(mSampleRate, 82.0e-9); // C37
    mBass.prepare(mSampleRate, 20.0, 0.5);
    mMiddle.prepare(mSampleRate, 20.0, 0.5);
    mTreble.prepare(mSampleRate, 20.0, 0.5);
    mHiTreble.prepare(mSampleRate, 20.0, 0.0);
    mVolume.prepare(mSampleRate, 20.0, 0.55);
    reset();
  }

  void reset() noexcept
  {
    mTrebleCap.reset();
    mBassCap.reset();
    mMiddleCap.reset();
    mHiTrebleCap.reset();
    mVolumeCouplingCap.reset();
  }

  void setControls(double bass,
                   double middle,
                   double treble,
                   double hiTreble,
                   double volume) noexcept
  {
    mBass.setTarget(clamp(bass, 0.0, 1.0));
    mMiddle.setTarget(clamp(middle, 0.0, 1.0));
    mTreble.setTarget(clamp(treble, 0.0, 1.0));
    mHiTreble.setTarget(clamp(hiTreble, 0.0, 1.0));
    mVolume.setTarget(clamp(volume, 0.0, 1.0));
  }

  double process(double input) noexcept
  {
    constexpr double kSlopeR = 100000.0;          // R74
    constexpr double kTreblePot = 250000.0;       // VR4 B
    constexpr double kBassPot = 250000.0;         // VR6 A
    constexpr double kMiddlePot = 10000.0;        // VR5 B
    constexpr double kBassSeriesR = 1000.0;       // R97
    constexpr double kHiTreblePot = 1000000.0;    // VR3 1M B rheostat
    constexpr double kTrebleToVolumeTopR = 100000.0; // R42
    constexpr double kVolumePot = 1000000.0;      // VR2 1M B
    constexpr double kPostVolumeLoad = 1000000.0; // R47
    constexpr double kMinSegmentR = 1.0;

    const double bass = mBass.next();
    const double middle = mMiddle.next();
    const double treble = mTreble.next();
    const double hiTreble = mHiTreble.next();
    const double volume = mVolume.next();

    // VR4 is B taper (linear). At 0 the wiper sits at the lower terminal
    // (bass-cap node); at 10 it sits at the C49/top terminal.
    const double trebleTopR = std::max(kMinSegmentR, kTreblePot * (1.0 - treble));
    const double trebleBottomR = std::max(kMinSegmentR, kTreblePot * treble);

    // Japanese A taper is the audio/log taper. A nominal 10 % midpoint is a
    // useful approximation where no manufacturer taper law is given.
    constexpr double kAudioTaperExponent = 3.321928094887362; // 0.5^x = 0.1
    const double bassLaw = std::pow(bass, kAudioTaperExponent);
    const double bassR = kBassSeriesR + kBassPot * bassLaw;

    // VR5 is a 10 k B (linear) rheostat to ground.
    const double middleR = std::max(kMinSegmentR, kMiddlePot * middle);

    // VR3 is a 1 M B rheostat in the C42 bright-bypass branch. Preserve the
    // schematic endpoints while using a perceptual law that spreads the most
    // useful part of its rotation across the knob.
    const double hiTrebleLaw = 1.0 - std::pow(1.0 - hiTreble, 2.0);
    const double hiTrebleR = std::max(kMinSegmentR, kHiTreblePot * (1.0 - hiTrebleLaw));

    // VR2 is B taper (linear), not an audio/log pot in this schematic.
    // volume=0 -> wiper at ground; volume=1 -> wiper at the top terminal.
    const double volumeUpperR = std::max(kMinSegmentR, kVolumePot * (1.0 - volume));
    const double volumeLowerR = std::max(kMinSegmentR, kVolumePot * volume);

    enum Node : int
    {
      kSlopeNode = 0,
      kTrebleTop,
      kBassNode,
      kMiddleNode,       // C51 / VR5 low-mid shunt node
      kTrebleWiper,      // VR4 wiper / tone-stack output / HI-TREBLE source
      kVolumeTop,        // isolated VR2 top, fed from VR4 wiper through R42
      kHiTrebleCapNode,
      kVolumeWiper,      // VR2 wiper / C42+VR3 bright-bypass destination
      kPostCoupling,     // after C37, loaded by R47
      kNodes
    };
    double a[kNodes][kNodes + 1]{};

    auto addToGround = [&a](int node, double conductance) noexcept
    {
      a[node][node] += conductance;
    };

    auto addBetween = [&a](int n1, int n2, double conductance) noexcept
    {
      a[n1][n1] += conductance;
      a[n2][n2] += conductance;
      a[n1][n2] -= conductance;
      a[n2][n1] -= conductance;
    };

    auto addFromKnownVoltage = [&a](int node, double conductance, double knownVoltage) noexcept
    {
      a[node][node] += conductance;
      a[node][kNodes] += conductance * knownVoltage;
    };

    // R74: source -> slope node.
    addFromKnownVoltage(kSlopeNode, 1.0 / kSlopeR, input);

    // C49: source -> top of treble pot. This keeps the already-calibrated
    // Bass/Middle/Treble reconstruction used by the previous build.
    const double trebleHistory = mTrebleCap.history();
    a[kTrebleTop][kTrebleTop] += mTrebleCap.conductance();
    a[kTrebleTop][kNodes] += mTrebleCap.conductance() * input - trebleHistory;

    // C50: slope node -> bass node.
    const double bassHistory = mBassCap.history();
    addBetween(kSlopeNode, kBassNode, mBassCap.conductance());
    a[kSlopeNode][kNodes] -= bassHistory;
    a[kBassNode][kNodes] += bassHistory;

    // C51: slope node -> middle/volume-top node.
    const double middleHistory = mMiddleCap.history();
    addBetween(kSlopeNode, kMiddleNode, mMiddleCap.conductance());
    a[kSlopeNode][kNodes] -= middleHistory;
    a[kMiddleNode][kNodes] += middleHistory;

    // VR4 treble divider, with its wiper as the high-frequency feed node.
    addBetween(kTrebleTop, kTrebleWiper, 1.0 / trebleTopR);
    addBetween(kTrebleWiper, kBassNode, 1.0 / trebleBottomR);

    // VR6 + R97, then VR5 to ground. This is the low/mid shunt branch of
    // the passive tone stack. Critically, this junction is NOT the top of VR2.
    addBetween(kBassNode, kMiddleNode, 1.0 / bassR);
    addToGround(kMiddleNode, 1.0 / middleR);

    // R42 (100 kOhm) carries the VR4 wiper (the tone-stack output) to an
    // isolated node at the TOP of VR2. Keeping this separate from the VR5
    // middle-shunt node restores the intended Bass/Treble interaction.
    addBetween(kTrebleWiper, kVolumeTop, 1.0 / kTrebleToVolumeTopR);

    // VR2 volume pot: its top is fed only through R42, its wiper feeds C37,
    // and its bottom terminal is grounded.
    addBetween(kVolumeTop, kVolumeWiper, 1.0 / volumeUpperR);
    addToGround(kVolumeWiper, 1.0 / volumeLowerR);

    // Only C42 + VR3 bypasses R42/part of VR2 and lands directly at the VR2
    // wiper. This is the actual HI-TREBLE/bright branch shown in the schematic.
    const double hiTrebleHistory = mHiTrebleCap.history();
    addBetween(kTrebleWiper, kHiTrebleCapNode, mHiTrebleCap.conductance());
    a[kTrebleWiper][kNodes] -= hiTrebleHistory;
    a[kHiTrebleCapNode][kNodes] += hiTrebleHistory;
    addBetween(kHiTrebleCapNode, kVolumeWiper, 1.0 / hiTrebleR);

    // C37 coupling into the IC3a input, loaded by R47 = 1 MOhm.
    const double couplingHistory = mVolumeCouplingCap.history();
    addBetween(kVolumeWiper, kPostCoupling, mVolumeCouplingCap.conductance());
    a[kVolumeWiper][kNodes] -= couplingHistory;
    a[kPostCoupling][kNodes] += couplingHistory;
    addToGround(kPostCoupling, 1.0 / kPostVolumeLoad);

    if(!solve<static_cast<std::size_t>(kNodes)>(a))
      return 0.0;

    const double slopeV = a[kSlopeNode][kNodes];
    const double trebleTopV = a[kTrebleTop][kNodes];
    const double bassV = a[kBassNode][kNodes];
    const double middleV = a[kMiddleNode][kNodes];
    const double trebleWiperV = a[kTrebleWiper][kNodes];
    const double hiTrebleCapV = a[kHiTrebleCapNode][kNodes];
    const double volumeWiperV = a[kVolumeWiper][kNodes];
    const double postCouplingV = a[kPostCoupling][kNodes];

    mTrebleCap.update(trebleTopV - input, trebleHistory);
    mBassCap.update(slopeV - bassV, bassHistory);
    mMiddleCap.update(slopeV - middleV, middleHistory);
    mHiTrebleCap.update(trebleWiperV - hiTrebleCapV, hiTrebleHistory);
    mVolumeCouplingCap.update(volumeWiperV - postCouplingV, couplingHistory);

    // With R42 connected to the TOP of VR2, the schematic itself now gives
    // HI-TREBLE its intended volume-dependent bright-bypass action. Do not add
    // a second digital shelf here: doing so would keep boosting treble even at
    // Volume 10, where the real bypass has little differential effect.
    return postCouplingV;
  }

private:
  class TrapezoidalCapacitor
  {
  public:
    void prepare(double sampleRate, double capacitance) noexcept
    {
      mG = 2.0 * capacitance * sampleRate;
      reset();
    }

    void reset() noexcept
    {
      mPreviousVoltage = 0.0;
      mPreviousCurrent = 0.0;
    }

    double conductance() const noexcept { return mG; }

    double history() const noexcept
    {
      return -mG * mPreviousVoltage - mPreviousCurrent;
    }

    void update(double voltage, double historyCurrent) noexcept
    {
      mPreviousCurrent = mG * voltage + historyCurrent;
      mPreviousVoltage = voltage;
    }

  private:
    double mG = 0.0;
    double mPreviousVoltage = 0.0;
    double mPreviousCurrent = 0.0;
  };

  template <std::size_t N>
  static bool solve(double (&a)[N][N + 1]) noexcept
  {
    constexpr int n = static_cast<int>(N);
    for(int column = 0; column < n; ++column)
    {
      int pivotRow = column;
      double pivotMagnitude = std::abs(a[pivotRow][column]);
      for(int row = column + 1; row < n; ++row)
      {
        const double magnitude = std::abs(a[row][column]);
        if(magnitude > pivotMagnitude)
        {
          pivotMagnitude = magnitude;
          pivotRow = row;
        }
      }

      if(pivotMagnitude < 1.0e-18)
        return false;

      if(pivotRow != column)
      {
        for(int c = column; c <= n; ++c)
          std::swap(a[column][c], a[pivotRow][c]);
      }

      const double pivot = a[column][column];
      for(int row = column + 1; row < n; ++row)
      {
        const double factor = a[row][column] / pivot;
        if(std::abs(factor) < 1.0e-30)
          continue;
        a[row][column] = 0.0;
        for(int c = column + 1; c <= n; ++c)
          a[row][c] -= factor * a[column][c];
      }
    }

    for(int row = n - 1; row >= 0; --row)
    {
      double rhs = a[row][n];
      for(int c = row + 1; c < n; ++c)
        rhs -= a[row][c] * a[c][n];
      const double diagonal = a[row][row];
      if(std::abs(diagonal) < 1.0e-18)
        return false;
      a[row][n] = rhs / diagonal;
    }

    return true;
  }

  double mSampleRate = 48000.0;
  TrapezoidalCapacitor mTrebleCap, mBassCap, mMiddleCap, mHiTrebleCap, mVolumeCouplingCap;
  SmoothedValue mBass, mMiddle, mTreble, mHiTreble, mVolume;
};

/**
 * IC3a post-volume amplifier.
 *
 * R46 = 270 kOhm feedback, R48 = 22 kOhm to ground, and C38 = 100 pF in
 * parallel with R46. The exact ideal-op-amp closed-loop transfer can be written
 * as:
 *
 *   H(s) = 1 + (R46/R48) / (1 + s R46 C38)
 *
 * so the stage is unity at very high frequency and 1 + R46/R48 at low
 * frequency. The previous implementation used a second-order low-pass on the
 * entire amplified signal, which attenuated the very frequencies HI-TREBLE is
 * meant to reveal.
 */
class VolumeStage
{
public:
  void prepare(double sampleRate)
  {
    mSampleRate = std::max(8000.0, sampleRate);
    constexpr double kFeedbackR = 270000.0; // R46
    constexpr double kFeedbackC = 100.0e-12; // C38
    const double cutoffHz = 1.0 / (2.0 * kPi * kFeedbackR * kFeedbackC);
    mFeedbackLowPass.prepare(mSampleRate, cutoffHz);
  }

  void reset() noexcept { mFeedbackLowPass.reset(); }

  // Volume itself is now solved in the passive VR2 network above. Keep this
  // method for call-site compatibility; there is no second volume attenuation.
  void setLevel(double) noexcept {}

  double process(double x) noexcept
  {
    constexpr double kFeedbackR = 270000.0; // R46
    constexpr double kGroundR = 22000.0;    // R48
    return x + (kFeedbackR / kGroundR) * mFeedbackLowPass.process(x);
  }

private:
  double mSampleRate = 48000.0;
  OnePoleLowPass mFeedbackLowPass;
};

} // namespace dtjb::dsp
