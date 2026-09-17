#include "dsp/SignalChain.h"
#include "ui/UIScaling.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>

int main()
{
  assert(dtjb::ui::CalculateMonitorFitScale(1920.0f, 1080.0f, 1.0f) == 1.0f);
  assert(dtjb::ui::CalculateMonitorFitScale(1920.0f, 1080.0f, 1.5f) == 1.0f);

  const float laptopScale = dtjb::ui::CalculateMonitorFitScale(1366.0f, 768.0f, 1.25f);
  assert(laptopScale < 0.9f && laptopScale > 0.88f);
  assert(1140.0f * laptopScale + 80.0f <= 1366.0f / 1.25f + 0.01f);

  const float compactScale = dtjb::ui::CalculateMonitorFitScale(1024.0f, 768.0f, 1.0f);
  assert(compactScale < 0.83f && compactScale > 0.82f);
  assert(1140.0f * compactScale + 80.0f <= 1024.01f);

  constexpr double sampleRates[] = {44100.0, 48000.0, 88200.0, 96000.0};

  for(const double sampleRate : sampleRates)
  {
    dtjb::dsp::SignalChain chain;
    chain.prepare(sampleRate);

    dtjb::dsp::Parameters parameters;
    parameters.distortionEnabled = true;
    parameters.distortion = 0.55;
    parameters.bass = 0.62;
    parameters.middle = 0.48;
    parameters.treble = 0.57;
    parameters.hiTreble = 0.75;
    parameters.volume = 0.42;
    parameters.reverb = 0.38;
    parameters.chorusMode = dtjb::dsp::BBDChorus::Mode::Manual;
    parameters.chorusRate = 0.37;
    parameters.chorusDepth = 0.66;
    parameters.oversampling = 4;
    chain.setParameters(parameters);

    double peak = 0.0;
    double stereoDifference = 0.0;
    const int frames = static_cast<int>(sampleRate * 2.0);
    for(int i = 0; i < frames; ++i)
    {
      const double t = static_cast<double>(i) / sampleRate;
      const double impulse = i == 0 ? 0.2 : 0.0;
      const double input = impulse + 0.018 * std::sin(2.0 * dtjb::dsp::kPi * 220.0 * t);
      const auto output = chain.process(input);
      assert(std::isfinite(output.left));
      assert(std::isfinite(output.right));
      peak = std::max(peak, std::max(std::abs(output.left), std::abs(output.right)));
      stereoDifference += std::abs(output.left - output.right);
    }

    assert(peak < 1.121);
    assert(stereoDifference > 0.001);
    std::cout << "sampleRate=" << sampleRate << " peak=" << peak
              << " stereoDifference=" << stereoDifference << '\n';
  }

  // The service schematic uses one interactive passive network, not three
  // independent EQ bands. The VR4 wiper is the tone-stack output and reaches
  // the isolated VR2 top through R42; the C51/VR5 node remains a shunt node.
  {
    constexpr double sampleRate = 48000.0;
    auto measureToneDb = [sampleRate](double frequency, double bass, double middle, double treble)
    {
      dtjb::dsp::ToneNetworkStage tone;
      tone.prepare(sampleRate);
      tone.setControls(bass, middle, treble, 0.0, 0.55);

      double sumSquares = 0.0;
      int count = 0;
      const int total = static_cast<int>(sampleRate);
      for(int i = 0; i < total; ++i)
      {
        const double x = 0.1 * std::sin(2.0 * dtjb::dsp::kPi * frequency * static_cast<double>(i) / sampleRate);
        const double y = tone.process(x);
        assert(std::isfinite(y));
        if(i > total / 2)
        {
          sumSquares += y * y;
          ++count;
        }
      }

      const double outputRms = std::sqrt(sumSquares / static_cast<double>(count));
      const double inputRms = 0.1 / std::sqrt(2.0);
      return 20.0 * std::log10(std::max(1.0e-12, outputRms / inputRms));
    };

    const double bassMin100 = measureToneDb(100.0, 0.0, 0.5, 0.5);
    const double bassMax100 = measureToneDb(100.0, 1.0, 0.5, 0.5);
    assert(bassMax100 - bassMin100 > 10.0);

    const double midMin400 = measureToneDb(400.0, 0.5, 0.0, 0.5);
    const double midMax400 = measureToneDb(400.0, 0.5, 1.0, 0.5);
    assert(midMax400 - midMin400 > 5.0);

    const double trebleMin4000 = measureToneDb(4000.0, 0.5, 0.5, 0.0);
    const double trebleMax4000 = measureToneDb(4000.0, 0.5, 0.5, 1.0);
    assert(trebleMax4000 - trebleMin4000 > 15.0);
  }

  // VR2 is a real 1 M B volume divider. The fixed 100 kOhm R42 feeds the
  // TOP of VR2; only C42+VR3 reaches the wiper. This regression test prevents
  // the old broadband-wiper bypass from returning: raising Volume must clearly
  // raise the midband level while the bright-bypass contribution becomes less
  // dominant, so the amp can become relatively darker at higher Volume.
  {
    constexpr double sampleRate = 48000.0;
    auto measureVolumeDb = [sampleRate](double frequency, double volume)
    {
      dtjb::dsp::ToneNetworkStage tone;
      tone.prepare(sampleRate);
      tone.setControls(0.5, 0.5, 0.5, 0.0, volume);

      double sumSquares = 0.0;
      int count = 0;
      const int total = static_cast<int>(sampleRate * 2.0);
      for(int i = 0; i < total; ++i)
      {
        const double x = 0.1 * std::sin(2.0 * dtjb::dsp::kPi * frequency * static_cast<double>(i) / sampleRate);
        const double y = tone.process(x);
        assert(std::isfinite(y));
        if(i > total / 2)
        {
          sumSquares += y * y;
          ++count;
        }
      }

      const double outputRms = std::sqrt(sumSquares / static_cast<double>(count));
      const double inputRms = 0.1 / std::sqrt(2.0);
      return 20.0 * std::log10(std::max(1.0e-12, outputRms / inputRms));
    };

    const double v0 = measureVolumeDb(1000.0, 0.0);
    const double v10 = measureVolumeDb(1000.0, 0.10);
    const double v25 = measureVolumeDb(1000.0, 0.25);
    const double v50 = measureVolumeDb(1000.0, 0.50);
    const double v75 = measureVolumeDb(1000.0, 0.75);
    const double v100 = measureVolumeDb(1000.0, 1.0);

    std::cerr << "volume 1kHz [0/1/2.5/5/7.5/10]="
              << v0 << '/' << v10 << '/' << v25 << '/'
              << v50 << '/' << v75 << '/' << v100 << " dB" << std::endl;

    assert(v0 < -100.0);
    assert(v10 < v25 && v25 < v50 && v50 < v75 && v75 < v100);
    assert(v100 - v50 > 4.0);
    assert(v100 - v10 > 12.0);

    const double tiltLowVolume = measureVolumeDb(8000.0, 0.25) - measureVolumeDb(1000.0, 0.25);
    const double tiltHighVolume = measureVolumeDb(8000.0, 1.0) - measureVolumeDb(1000.0, 1.0);
    std::cerr << "volume spectral tilt 8k-1k: low=" << tiltLowVolume
              << "dB high=" << tiltHighVolume << "dB" << std::endl;
    assert(std::abs(tiltLowVolume - tiltHighVolume) < 1.5);
  }

  // VR3 (HI-TREBLE) is a 1 M B rheostat in series with C42 (470 pF),
  // creating a progressive high-frequency bypass from the VR4 wiper directly
  // to the VR2 wiper. R42 (100 kOhm) instead feeds the TOP of VR2. The circuit
  // itself now provides the intended action, so no extra digital treble shelf
  // is required.
  {
    constexpr double sampleRate = 48000.0;
    auto measure = [sampleRate](double frequency, double hiTreble)
    {
      dtjb::dsp::ToneNetworkStage tone;
      tone.prepare(sampleRate);
      tone.setControls(0.5, 0.5, 0.5, hiTreble, 0.55);

      double sumSquares = 0.0;
      int count = 0;
      const int total = static_cast<int>(sampleRate);
      for(int i = 0; i < total; ++i)
      {
        const double x = 0.1 * std::sin(2.0 * dtjb::dsp::kPi * frequency * static_cast<double>(i) / sampleRate);
        const double y = tone.process(x);
        assert(std::isfinite(y));
        if(i > total / 2)
        {
          sumSquares += y * y;
          ++count;
        }
      }

      return std::sqrt(sumSquares / static_cast<double>(count));
    };

    const double lowOff = measure(200.0, 0.0);
    const double lowMax = measure(200.0, 1.0);
    const double highOff = measure(8000.0, 0.0);
    const double highQuarter = measure(8000.0, 0.25);
    const double highMid = measure(8000.0, 0.5);
    const double highThreeQuarter = measure(8000.0, 0.75);
    const double highMax = measure(8000.0, 1.0);

    const double lowDeltaDb = 20.0 * std::log10(std::max(1.0e-12, lowMax / lowOff));
    const double quarterDeltaDb = 20.0 * std::log10(std::max(1.0e-12, highQuarter / highOff));
    const double midDeltaDb = 20.0 * std::log10(std::max(1.0e-12, highMid / highOff));
    const double threeQuarterDeltaDb = 20.0 * std::log10(std::max(1.0e-12, highThreeQuarter / highOff));
    const double highDeltaDb = 20.0 * std::log10(std::max(1.0e-12, highMax / highOff));
    std::cerr << "hiTreble isolated: 200Hz=" << lowDeltaDb
              << "dB, 8kHz [25/50/75/100]="
              << quarterDeltaDb << '/' << midDeltaDb << '/'
              << threeQuarterDeltaDb << '/' << highDeltaDb << "dB" << std::endl;
    assert(std::abs(lowDeltaDb) < 0.5);
    assert(highDeltaDb > 4.0);
    assert(highOff < highQuarter && highQuarter < highMid);
    assert(highMid < highThreeQuarter && highThreeQuarter < highMax);
    assert(midDeltaDb > 2.0);
  }

  // Verify the corrected VR2-wiper topology survives the complete signal path
  // (not just the isolated tone network). At the default Volume 5.5, maximum
  // HI-TREBLE should be clearly measurable in the upper treble without acting
  // like a broadband gain control.
  {
    constexpr double sampleRate = 48000.0;
    auto measureFullPath = [sampleRate](double frequency, double hiTreble)
    {
      dtjb::dsp::SignalChain chain;
      chain.prepare(sampleRate);
      dtjb::dsp::Parameters p;
      p.bass = 0.5;
      p.middle = 0.5;
      p.treble = 0.5;
      p.hiTreble = hiTreble;
      p.volume = 0.55;
      p.reverb = 0.0;
      p.chorusMode = dtjb::dsp::BBDChorus::Mode::Off;
      chain.setParameters(p);

      double sumSquares = 0.0;
      int count = 0;
      const int total = static_cast<int>(sampleRate * 2.0);
      for(int i = 0; i < total; ++i)
      {
        const double x = 0.005 * std::sin(2.0 * dtjb::dsp::kPi * frequency * static_cast<double>(i) / sampleRate);
        const auto y = chain.process(x);
        if(i > total / 2)
        {
          sumSquares += y.left * y.left;
          ++count;
        }
      }
      return std::sqrt(sumSquares / static_cast<double>(count));
    };

    const double lowDeltaDb = 20.0 * std::log10(measureFullPath(200.0, 1.0) / measureFullPath(200.0, 0.0));
    const double presenceDeltaDb = 20.0 * std::log10(measureFullPath(4000.0, 1.0) / measureFullPath(4000.0, 0.0));
    const double highDeltaDb = 20.0 * std::log10(measureFullPath(8000.0, 1.0) / measureFullPath(8000.0, 0.0));
    const double airDeltaDb = 20.0 * std::log10(measureFullPath(12000.0, 1.0) / measureFullPath(12000.0, 0.0));
    std::cerr << "hiTreble full path: 200Hz=" << lowDeltaDb
              << "dB, 4/8/12kHz=" << presenceDeltaDb << '/'
              << highDeltaDb << '/' << airDeltaDb << "dB" << std::endl;
    assert(std::abs(lowDeltaDb) < 0.5);
    assert(presenceDeltaDb > 3.5);
    assert(highDeltaDb > 4.0);
    assert(airDeltaDb > 4.0);
  }

  // First verify the three selectable local-oversampling modes themselves.
  // This catches a selector that changes in the UI but not in the DSP.
  {
    dtjb::dsp::LocalOversampler oversampler;
    int evaluations = 0;
    const auto nonlinear = [&evaluations](double x) noexcept
    {
      ++evaluations;
      return std::tanh(x);
    };

    oversampler.process(0.75, 1, nonlinear);
    assert(evaluations == 1);
    oversampler.reset();
    evaluations = 0;
    oversampler.process(0.75, 2, nonlinear);
    assert(evaluations == 2);
    oversampler.reset();
    evaluations = 0;
    oversampler.process(0.75, 4, nonlinear);
    assert(evaluations == 4);
  }

  // Even with distortion disabled and the power amp below compression, the
  // selector must still reach both nonlinear preamp devices. This prevents a
  // seemingly inert x1/x2/x4 control under normal playing conditions.
  {
    constexpr double sampleRate = 48000.0;
    auto x1 = std::make_unique<dtjb::dsp::SignalChain>();
    auto x2 = std::make_unique<dtjb::dsp::SignalChain>();
    auto x4 = std::make_unique<dtjb::dsp::SignalChain>();
    x1->prepare(sampleRate);
    x2->prepare(sampleRate);
    x4->prepare(sampleRate);

    dtjb::dsp::Parameters p;
    p.distortionEnabled = false;
    p.volume = 0.25;
    p.reverb = 0.0;
    p.chorusMode = dtjb::dsp::BBDChorus::Mode::Off;
    p.oversampling = 1;
    x1->setParameters(p);
    p.oversampling = 2;
    x2->setParameters(p);
    p.oversampling = 4;
    x4->setParameters(p);

    double cleanDifference = 0.0;
    for(int i = 0; i < static_cast<int>(sampleRate * 0.25); ++i)
    {
      const double t = static_cast<double>(i) / sampleRate;
      const double input = 0.0005 * std::sin(2.0 * dtjb::dsp::kPi * 997.0 * t);
      const auto y1 = x1->process(input);
      const auto y2 = x2->process(input);
      const auto y4 = x4->process(input);
      cleanDifference += std::abs(y1.left - y2.left) + std::abs(y2.left - y4.left);
    }
    std::cerr << "oversampling normal-level difference=" << cleanDifference << std::endl;
    assert(cleanDifference > 1.0e-8);
  }

  // Under deliberate nonlinear drive, x1/x2/x4 must produce measurably
  // different results. Distortion is the always-local nonlinear test case;
  // the preamp always follows the selection and the power amp additionally
  // engages oversampling near clipping.
  {
    constexpr double sampleRate = 48000.0;
    auto x1 = std::make_unique<dtjb::dsp::SignalChain>();
    auto x2 = std::make_unique<dtjb::dsp::SignalChain>();
    auto x4 = std::make_unique<dtjb::dsp::SignalChain>();
    x1->prepare(sampleRate);
    x2->prepare(sampleRate);
    x4->prepare(sampleRate);

    dtjb::dsp::Parameters p;
    p.inputTrimDb = 12.0;
    p.distortionEnabled = true;
    p.distortion = 1.0;
    p.volume = 0.85;
    p.reverb = 0.0;
    p.chorusMode = dtjb::dsp::BBDChorus::Mode::Off;
    p.oversampling = 1;
    x1->setParameters(p);
    p.oversampling = 2;
    x2->setParameters(p);
    p.oversampling = 4;
    x4->setParameters(p);

    double difference12 = 0.0;
    double difference24 = 0.0;
    for(int i = 0; i < static_cast<int>(sampleRate * 0.5); ++i)
    {
      const double t = static_cast<double>(i) / sampleRate;
      const double input = 0.12 * std::sin(2.0 * dtjb::dsp::kPi * 997.0 * t) +
                           0.04 * std::sin(2.0 * dtjb::dsp::kPi * 9100.0 * t);
      const auto y1 = x1->process(input);
      const auto y2 = x2->process(input);
      const auto y4 = x4->process(input);
      difference12 += std::abs(y1.left - y2.left);
      difference24 += std::abs(y2.left - y4.left);
    }
    std::cerr << "oversampling nonlinear differences: x1/x2=" << difference12
              << ", x2/x4=" << difference24 << std::endl;
    assert(difference12 > 1.0e-4);
    assert(difference24 > 1.0e-5);
  }

  // Output Trim is the final stereo gain stage. Verify both ends of the
  // requested range after its click-free smoothing has settled.
  {
    constexpr double sampleRate = 48000.0;
    auto unity = std::make_unique<dtjb::dsp::SignalChain>();
    auto cut = std::make_unique<dtjb::dsp::SignalChain>();
    auto boost = std::make_unique<dtjb::dsp::SignalChain>();
    unity->prepare(sampleRate);
    cut->prepare(sampleRate);
    boost->prepare(sampleRate);

    dtjb::dsp::Parameters unityParameters;
    dtjb::dsp::Parameters cutParameters;
    dtjb::dsp::Parameters boostParameters;
    cutParameters.outputTrimDb = -24.0;
    boostParameters.outputTrimDb = 12.0;
    unity->setParameters(unityParameters);
    cut->setParameters(cutParameters);
    boost->setParameters(boostParameters);

    const double cutGain = dtjb::dsp::dbToGain(-24.0);
    const double boostGain = dtjb::dsp::dbToGain(12.0);
    int comparedFrames = 0;
    for(int i = 0; i < static_cast<int>(sampleRate * 0.5); ++i)
    {
      const double t = static_cast<double>(i) / sampleRate;
      const double input = 0.012 * std::sin(2.0 * dtjb::dsp::kPi * 330.0 * t);
      const auto unityOut = unity->process(input);
      const auto cutOut = cut->process(input);
      const auto boostOut = boost->process(input);

      if(i > static_cast<int>(sampleRate * 0.4) && std::abs(unityOut.left) > 1.0e-7)
      {
        assert(std::abs(cutOut.left - unityOut.left * cutGain) < 1.0e-6);
        assert(std::abs(boostOut.left - unityOut.left * boostGain) < 1.0e-6);
        ++comparedFrames;
      }
    }
    assert(comparedFrames > 1000);
  }

  // A lightweight whole-chain benchmark makes performance regressions and an
  // accidentally inert mode visible without imposing machine-specific timing
  // assertions. Use the best of two runs to reduce scheduler noise.
  {
    constexpr double sampleRate = 48000.0;
    constexpr int measuredFrames = 500000;
    std::array<double, 256> inputTable{};
    for(std::size_t i = 0; i < inputTable.size(); ++i)
      inputTable[i] = 0.018 * std::sin(2.0 * dtjb::dsp::kPi * static_cast<double>(i) /
                                      static_cast<double>(inputTable.size()));

    const auto measureMilliseconds = [&](int factor)
    {
      double best = 1.0e30;
      volatile double checksum = 0.0;
      for(int trial = 0; trial < 2; ++trial)
      {
        auto chain = std::make_unique<dtjb::dsp::SignalChain>();
        chain->prepare(sampleRate);
        dtjb::dsp::Parameters p;
        p.oversampling = factor;
        p.chorusMode = dtjb::dsp::BBDChorus::Mode::Off;
        chain->setParameters(p);

        for(int i = 0; i < 4096; ++i)
          chain->process(inputTable[static_cast<std::size_t>(i) & 255U]);

        const auto start = std::chrono::steady_clock::now();
        double sum = 0.0;
        for(int i = 0; i < measuredFrames; ++i)
        {
          const double input = inputTable[static_cast<std::size_t>(i) & 255U];
          const auto output = chain->process(input);
          sum += output.left + output.right;
        }
        const auto end = std::chrono::steady_clock::now();
        checksum += sum;
        best = std::min(best, std::chrono::duration<double, std::milli>(end - start).count());
      }
      assert(std::isfinite(static_cast<double>(checksum)));
      return best;
    };

    const double x1Milliseconds = measureMilliseconds(1);
    const double x2Milliseconds = measureMilliseconds(2);
    const double x4Milliseconds = measureMilliseconds(4);
    std::cerr << "whole-chain benchmark: x1=" << x1Milliseconds
              << "ms, x2=" << x2Milliseconds
              << "ms, x4=" << x4Milliseconds
              << "ms, x4/x1=" << x4Milliseconds / x1Milliseconds << 'x' << std::endl;
  }

  return 0;
}
