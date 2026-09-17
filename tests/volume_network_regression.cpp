#include "dsp/ToneAndVolume.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {
double MeasureToneDb(double frequency, double volume, double hiTreble = 0.0)
{
  constexpr double sampleRate = 48000.0;
  dtjb::dsp::ToneNetworkStage tone;
  tone.prepare(sampleRate);
  tone.setControls(0.5, 0.5, 0.5, hiTreble, volume);

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
}
} // namespace

int main()
{
  const double v0 = MeasureToneDb(1000.0, 0.0);
  const double v10 = MeasureToneDb(1000.0, 0.10);
  const double v25 = MeasureToneDb(1000.0, 0.25);
  const double v50 = MeasureToneDb(1000.0, 0.50);
  const double v75 = MeasureToneDb(1000.0, 0.75);
  const double v100 = MeasureToneDb(1000.0, 1.0);

  std::cout << "Volume 1 kHz [0/1/2.5/5/7.5/10] dB: "
            << v0 << ' ' << v10 << ' ' << v25 << ' '
            << v50 << ' ' << v75 << ' ' << v100 << '\n';

  assert(v0 < -100.0);
  assert(v10 < v25 && v25 < v50 && v50 < v75 && v75 < v100);
  assert(v100 - v50 > 4.0);
  assert(v100 - v10 > 12.0);

  const double lowVolumeTilt = MeasureToneDb(8000.0, 0.25) - MeasureToneDb(1000.0, 0.25);
  const double highVolumeTilt = MeasureToneDb(8000.0, 1.0) - MeasureToneDb(1000.0, 1.0);
  std::cout << "8 kHz - 1 kHz tilt, Volume 2.5 / 10 dB: "
            << lowVolumeTilt << ' ' << highVolumeTilt << '\n';
  // With R42 feeding an isolated VR2-top node, Volume is primarily a level
  // control; it must not collapse Bass/Treble by tying VR2 into the VR5 shunt.
  assert(std::abs(lowVolumeTilt - highVolumeTilt) < 1.5);

  // C42+VR3 remains a volume-dependent bright bypass: strongest at mid/low
  // Volume and much weaker as the VR2 top and wiper approach one another.
  const double midHiTreble = MeasureToneDb(8000.0, 0.55, 1.0) - MeasureToneDb(8000.0, 0.55, 0.0);
  const double maxHiTreble = MeasureToneDb(8000.0, 1.0, 1.0) - MeasureToneDb(8000.0, 1.0, 0.0);
  std::cout << "HI-TREBLE 8 kHz delta at Volume 5.5 / 10 dB: "
            << midHiTreble << ' ' << maxHiTreble << '\n';
  assert(midHiTreble > 4.0);
  assert(std::abs(maxHiTreble) < 1.5);

  return 0;
}
