#pragma once

namespace iplug { namespace igraphics { class IGraphics; } }

namespace dtjb::ui
{
  // This is the single shared layout implementation used by both APP and VST3.
  // iPlug2 compiles this same source in each target with the proper API context.
  void BuildJazzBeatUI(iplug::igraphics::IGraphics* graphics);
}
