#pragma once

#include "JazzBeatPresetFormat.h"
#include "../Parameters.h"

#include <array>
#include <cmath>
#include <cstdint>

namespace dtjb::preset {

struct ParamSpec {
  std::uint32_t stableId;
  int paramIdx;
  ValueType type;
  double minValue;
  double maxValue;
};

static constexpr std::array<ParamSpec, 15> kParamSpecs {{
  {0x2101u, kInputMode,     ValueType::Enumeration, 0.0,  1.0},
  {0x2102u, kInputTrim,     ValueType::Continuous, -24.0, 24.0},
  {0x2103u, kDistortionOn,  ValueType::Toggle,      0.0,  1.0},
  {0x2104u, kDistortion,    ValueType::Continuous,  0.0,100.0},
  {0x2110u, kBass,          ValueType::Continuous,  0.0, 10.0},
  {0x2111u, kMiddle,        ValueType::Continuous,  0.0, 10.0},
  {0x2112u, kTreble,        ValueType::Continuous,  0.0, 10.0},
  {0x2113u, kVolume,        ValueType::Continuous,  0.0, 10.0},
  {0x2120u, kReverb,        ValueType::Continuous,  0.0, 10.0},
  {0x2130u, kChorusMode,    ValueType::Enumeration, 0.0,  2.0},
  {0x2131u, kChorusRate,    ValueType::Continuous,  0.0,100.0},
  {0x2132u, kChorusDepth,   ValueType::Continuous,  0.0,100.0},
  {0x2140u, kOversampling,  ValueType::Enumeration, 0.0,  2.0},
  {0x2141u, kOutputTrim,    ValueType::Continuous, -24.0, 12.0},
  {0x2150u, kHiTreble,      ValueType::Continuous,  0.0, 10.0}
}};

static_assert(kParamSpecs.size() == static_cast<std::size_t>(kNumParams - 1),
              "Jazz Beat preset map must include every user-facing parameter except kHiTrebleLegacy");

constexpr bool ContainsParamIndex(int paramIdx) {
  for (const auto& spec : kParamSpecs)
    if (spec.paramIdx == paramIdx) return true;
  return false;
}
static_assert(!ContainsParamIndex(kHiTrebleLegacy),
              "Legacy boolean Hi-Treble ParamID 7 must never be serialized to .dtjbpreset");
static_assert(ContainsParamIndex(kHiTreble),
              "Continuous Hi-Treble must be serialized to .dtjbpreset");

inline const ParamSpec* FindSpec(std::uint32_t stableId) {
  for (const auto& spec : kParamSpecs)
    if (spec.stableId == stableId) return &spec;
  return nullptr;
}

inline bool ValueValid(const ParamSpec& spec, double value) {
  if (!std::isfinite(value) || value < spec.minValue - 1e-9 || value > spec.maxValue + 1e-9)
    return false;
  if (spec.type == ValueType::Toggle)
    return std::abs(value) < 1e-9 || std::abs(value - 1.0) < 1e-9;
  if (spec.type == ValueType::Enumeration)
    return std::abs(value - std::round(value)) < 1e-9;
  return true;
}

} // namespace dtjb::preset
