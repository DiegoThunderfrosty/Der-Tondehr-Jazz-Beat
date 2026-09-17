#include "dsp/ToneAndVolume.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {
double Measure(double frequency, double bass, double middle, double treble)
{
  constexpr double sampleRate = 48000.0;
  dtjb::dsp::ToneNetworkStage tone;
  tone.prepare(sampleRate);
  tone.setControls(bass, middle, treble, 0.0, 0.55);

  double sumSquares = 0.0;
  int count = 0;
  constexpr int total = 96000;
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
}
} // namespace

int main()
{
  const double bassDelta100 = Measure(100.0, 1.0, 0.5, 0.5) - Measure(100.0, 0.0, 0.5, 0.5);
  const double middleDelta400 = Measure(400.0, 0.5, 1.0, 0.5) - Measure(400.0, 0.5, 0.0, 0.5);
  const double trebleDelta4k = Measure(4000.0, 0.5, 0.5, 1.0) - Measure(4000.0, 0.5, 0.5, 0.0);
  const double trebleDelta8k = Measure(8000.0, 0.5, 0.5, 1.0) - Measure(8000.0, 0.5, 0.5, 0.0);

  std::cout << "Tone deltas dB: Bass@100=" << bassDelta100
            << " Middle@400=" << middleDelta400
            << " Treble@4k=" << trebleDelta4k
            << " Treble@8k=" << trebleDelta8k << '\n';

  // These thresholds deliberately sit below the measured response but high
  // enough to catch the 0.1.27 regression where the VR5 shunt node was
  // accidentally merged with VR2 top and Bass/Treble became nearly inert.
  assert(bassDelta100 > 10.0);
  assert(middleDelta400 > 5.0);
  assert(trebleDelta4k > 15.0);
  assert(trebleDelta8k > 18.0);
  return 0;
}
