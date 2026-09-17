#pragma once

#include <algorithm>

namespace dtjb::ui
{
inline float CalculateMonitorFitScale(float workWidthPixels,
                                      float workHeightPixels,
                                      float dpiScale) noexcept
{
  dpiScale = std::max(dpiScale, 0.5f);
  const float logicalWorkWidth = workWidthPixels / dpiScale;
  const float logicalWorkHeight = workHeightPixels / dpiScale;
  const float widthScale = (logicalWorkWidth - 80.0f) / 1140.0f;
  const float heightScale = (logicalWorkHeight - 100.0f) / 510.0f;
  return std::clamp(std::min({1.0f, widthScale, heightScale}), 0.5f, 1.0f);
}
} // namespace dtjb::ui
