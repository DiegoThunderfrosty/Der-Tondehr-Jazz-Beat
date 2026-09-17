#include "JazzBeatUI.h"
#include "../Parameters.h"
#include "IControls.h"
#include <algorithm>

using namespace iplug;
using namespace igraphics;

namespace dtjb::ui
{
namespace
{
class JazzBeat3DChassisControl final : public IControl
{
public:
  explicit JazzBeat3DChassisControl(const IRECT& bounds)
  : IControl(bounds)
  {
    SetIgnoreMouse(true);
  }

  void Draw(IGraphics& g) override
  {
    const IColor shadowSoft(68, 0, 0, 0);
    const IColor shadowHard(120, 0, 0, 0);
    const IColor shellEdge(255, 72, 69, 60);
    const IColor shellDark(255, 20, 22, 22);
    const IColor shellFace(255, 30, 32, 31);
    const IColor shellInset(255, 24, 26, 25);
    const IColor bayTop(255, 37, 40, 39);
    const IColor bayBottom(255, 26, 28, 27);
    const IColor accent(170, 201, 83, 43);
    const IColor creamGlow(110, 232, 222, 192);
    const IColor dividerDark(120, 10, 10, 10);
    const IColor dividerLight(90, 205, 190, 160);

    const IRECT outer = mRECT.GetPadded(-6.f);
    g.FillRoundRect(shadowSoft, outer.GetTranslated(0.f, 7.f), 12.f);
    g.FillRoundRect(shadowHard, outer.GetTranslated(0.f, 3.f), 12.f);
    g.FillRoundRect(shellDark, outer, 12.f);

    const IRECT shell = outer.GetPadded(-1.2f);
    g.FillRoundRect(shellFace, shell, 10.5f);
    g.DrawRoundRect(shellEdge, shell, 10.5f, nullptr, 1.0f);
    g.DrawLine(creamGlow, shell.L + 12.f, shell.T + 1.2f, shell.R - 12.f, shell.T + 1.2f, nullptr, 1.0f);
    g.DrawLine(IColor(140, 0, 0, 0), shell.L + 10.f, shell.B - 1.1f, shell.R - 10.f, shell.B - 1.1f, nullptr, 1.2f);

    // Header plate
    DrawRaisedBay(g, IRECT(306.f, 51.f, 830.f, 79.f), 4.5f, bayTop, bayBottom, shellEdge, accent);

    // Main amplifier rack face.
    DrawRaisedBay(g, IRECT(18.f, 80.f, 1118.f, 430.f), 2.5f, bayTop, bayBottom, shellEdge, accent);

    // Per-section subtle bays to create depth without changing layout.
    DrawInsetBay(g, IRECT(27.f, 111.f, 119.f, 396.f), 4.5f, shellInset, bayTop, accent);
    DrawInsetBay(g, IRECT(135.f, 111.f, 227.f, 396.f), 4.5f, shellInset, bayTop, accent);
    DrawInsetBay(g, IRECT(243.f, 111.f, 659.f, 396.f), 5.0f, shellInset, bayTop, accent);
    DrawInsetBay(g, IRECT(675.f, 111.f, 767.f, 396.f), 4.5f, shellInset, bayTop, accent);
    DrawInsetBay(g, IRECT(783.f, 111.f, 983.f, 396.f), 5.0f, shellInset, bayTop, accent);
    DrawInsetBay(g, IRECT(999.f, 111.f, 1091.f, 396.f), 4.5f, shellInset, bayTop, accent);

    // Lower control shelves.
    DrawRaisedBay(g, IRECT(27.f, 317.f, 119.f, 365.f), 2.0f, bayTop, bayBottom, shellEdge, accent);
    DrawRaisedBay(g, IRECT(135.f, 317.f, 227.f, 365.f), 2.0f, bayTop, bayBottom, shellEdge, accent);
    DrawRaisedBay(g, IRECT(243.f, 317.f, 443.f, 365.f), 2.0f, bayTop, bayBottom, shellEdge, accent);
    DrawInsetBay(g, IRECT(454.f, 305.f, 540.f, 386.f), 4.0f, shellInset, bayTop, accent);
    DrawRaisedBay(g, IRECT(783.f, 317.f, 983.f, 365.f), 2.0f, bayTop, bayBottom, shellEdge, accent);

    // Divider highlight/shadow pairs so the separators feel engraved.
    for (const float x : {119.f, 227.f, 659.f, 767.f, 983.f}) {
      g.DrawLine(dividerDark, x + 1.0f, 92.f, x + 1.0f, 396.f, nullptr, 1.4f);
      g.DrawLine(dividerLight, x, 91.f, x, 396.f, nullptr, 0.7f);
    }

    // Premium mounting cups below each main rotary control. These are drawn
    // under the real iPlug2 knobs, so the controls still animate exactly as
    // before while visually sitting in machined metal sockets.
    constexpr float kLeft = 27.0f;
    constexpr float kPitch = 108.0f;
    constexpr float kCellW = 96.0f;
    constexpr float kKnobSize = 84.0f;
    constexpr float kKnobTop = 149.0f;
    for (int column = 0; column < 10; ++column) {
      const float left = kLeft + static_cast<float>(column) * kPitch;
      const float knobLeft = left + (kCellW - kKnobSize) * 0.5f;
      const float cx = knobLeft + kKnobSize * 0.5f;
      const float cy = kKnobTop + kKnobSize * 0.5f;
      DrawKnobSocket(g, cx, cy, 44.5f, accent);

      // Small recessed value window beneath each knob.
      const IRECT valueWell(left + 5.f, 246.f, left + kCellW - 5.f, 273.f);
      DrawValueWell(g, valueWell);
    }

    // Hi-Treble miniature control gets the same machined treatment.
    DrawKnobSocket(g, 497.f, 334.f, 29.0f, accent);
    DrawValueWell(g, IRECT(459.f, 361.f, 535.f, 386.f));

    // Two thin top rails mimic a brushed anodized faceplate and add a stronger
    // premium hierarchy to the front panel.
    g.DrawLine(IColor(145, 245, 235, 205), 31.f, 84.f, 1105.f, 84.f, nullptr, 0.75f);
    g.DrawLine(IColor(110, 201, 83, 43), 33.f, 87.f, 1103.f, 87.f, nullptr, 0.60f);

    // Only four fasteners: enough to sell the physical rack without littering
    // the control surface with decorative dots.
    DrawScrew(g, 27.f, 91.f);
    DrawScrew(g, 1109.f, 91.f);
    DrawScrew(g, 27.f, 419.f);
    DrawScrew(g, 1109.f, 419.f);
  }

private:
  static void DrawKnobSocket(IGraphics& g, float cx, float cy, float radius, IColor accent)
  {
    g.FillCircle(IColor(82, 0, 0, 0), cx + 1.6f, cy + 3.2f, radius + 3.0f);
    g.FillCircle(IColor(255, 14, 16, 16), cx, cy, radius + 2.0f);
    g.DrawCircle(IColor(175, 92, 87, 73), cx, cy, radius + 1.0f, nullptr, 1.25f);
    g.DrawCircle(IColor(150, 241, 230, 199), cx - 0.35f, cy - 0.45f, radius - 0.4f, nullptr, 0.85f);
    g.DrawCircle(IColor(105, accent.R, accent.G, accent.B), cx, cy, radius - 2.0f, nullptr, 0.70f);
    g.FillCircle(IColor(35, 255, 255, 255), cx - radius * 0.25f, cy - radius * 0.30f, radius * 0.14f);
  }

  static void DrawValueWell(IGraphics& g, const IRECT& r)
  {
    g.FillRoundRect(IColor(110, 0, 0, 0), r.GetTranslated(0.f, 1.4f), 2.5f);
    g.FillRoundRect(IColor(255, 20, 22, 21), r, 2.5f);
    const IRECT face = r.GetPadded(-1.0f);
    g.FillRoundRect(IColor(255, 29, 31, 30), face, 1.8f);
    g.DrawLine(IColor(120, 0, 0, 0), face.L + 3.f, face.T + 0.8f, face.R - 3.f, face.T + 0.8f, nullptr, 0.8f);
    g.DrawLine(IColor(70, 224, 214, 185), face.L + 3.f, face.B - 0.8f, face.R - 3.f, face.B - 0.8f, nullptr, 0.55f);
  }

  static void DrawScrew(IGraphics& g, float x, float y)
  {
    g.FillCircle(IColor(105, 0, 0, 0), x + 1.0f, y + 1.4f, 3.2f);
    g.FillCircle(IColor(255, 19, 21, 20), x, y, 3.0f);
    g.FillCircle(IColor(255, 106, 102, 86), x - 0.25f, y - 0.35f, 2.15f);
    g.FillCircle(IColor(180, 244, 235, 205), x - 0.85f, y - 0.95f, 0.62f);
    g.DrawLine(IColor(210, 31, 32, 29), x - 1.25f, y + 0.9f, x + 1.25f, y - 0.9f, nullptr, 0.65f);
  }

  static void DrawRaisedBay(IGraphics& g, const IRECT& r, float radius,
                            IColor top, IColor bottom, IColor edge, IColor accent)
  {
    g.FillRoundRect(IColor(90, 0, 0, 0), r.GetTranslated(0.f, 3.f), radius);
    g.FillRoundRect(bottom, r, radius);
    const IRECT face = r.GetPadded(-1.0f);
    g.FillRoundRect(top, face, std::max(0.f, radius - 0.8f));
    g.DrawRoundRect(edge, face, std::max(0.f, radius - 0.8f), nullptr, 0.8f);
    g.DrawLine(IColor(120, 240, 233, 213), face.L + radius, face.T + 1.f,
               face.R - radius, face.T + 1.f, nullptr, 0.8f);
    g.DrawLine(IColor(140, 0, 0, 0), face.L + radius, face.B - 1.f,
               face.R - radius, face.B - 1.f, nullptr, 1.0f);
    g.DrawLine(IColor(80, accent.R, accent.G, accent.B), face.L + radius + 4.f, face.T + 2.2f,
               face.R - radius - 4.f, face.T + 2.2f, nullptr, 0.5f);
  }

  static void DrawInsetBay(IGraphics& g, const IRECT& r, float radius,
                           IColor dark, IColor inner, IColor accent)
  {
    g.FillRoundRect(IColor(170, 17, 18, 18), r, radius);
    IRECT trench = r.GetPadded(-1.2f);
    g.FillRoundRect(dark, trench, std::max(0.f, radius - 0.8f));
    g.DrawLine(IColor(160, 0, 0, 0), trench.L + radius, trench.T + 1.0f,
               trench.R - radius, trench.T + 1.0f, nullptr, 1.0f);
    IRECT face = trench.GetPadded(-2.f);
    g.FillRoundRect(inner, face, std::max(0.f, radius - 2.0f));
    g.DrawRoundRect(IColor(90, 84, 79, 68), face, std::max(0.f, radius - 2.0f), nullptr, 0.65f);
    g.DrawLine(IColor(55, accent.R, accent.G, accent.B), face.L + radius, face.T + 1.2f,
               face.R - radius, face.T + 1.2f, nullptr, 0.45f);
  }
};

class JazzBeatKnob3DOverlayControl final : public IControl
{
public:
  explicit JazzBeatKnob3DOverlayControl(const IRECT& bounds)
  : IControl(bounds)
  {
    SetIgnoreMouse(true);
  }

  void Draw(IGraphics& g) override
  {
    constexpr float kLeft = 27.0f;
    constexpr float kPitch = 108.0f;
    constexpr float kCellW = 96.0f;
    constexpr float kKnobSize = 84.0f;
    constexpr float kKnobTop = 149.0f;
    auto columnX = [&](int column) {
      return kLeft + static_cast<float>(column) * kPitch;
    };

    const int columns[] = {0,1,2,3,4,5,6,7,8,9};
    for (const int column : columns) {
      const float left = columnX(column);
      const float knobLeft = left + (kCellW - kKnobSize) * 0.5f;
      DrawKnobOverlay(g, IRECT(knobLeft, kKnobTop, knobLeft + kKnobSize, kKnobTop + kKnobSize), 0.0f);
    }

    // Hi-Treble uses a smaller knob in column 4. Align the premium overlay
    // with the exact knob bounds so the effect sits on top of the control
    // instead of drifting to the left.
    DrawKnobOverlay(g, IRECT(480.f, 307.f, 534.f, 361.f), -0.05f);
  }

private:
  static void DrawKnobOverlay(IGraphics& g, const IRECT& r, float brightnessBias)
  {
    const float cx = static_cast<float>(r.MW());
    const float cy = static_cast<float>(r.MH());
    const float radius = std::min(static_cast<float>(r.W()), static_cast<float>(r.H())) * 0.49f;

    // Dense contact shadow makes the control read as a physical object raised
    // several millimetres from the fascia.
    g.FillCircle(IColor(62, 0, 0, 0), cx + 1.5f, cy + radius * 0.17f, radius + 2.8f);
    g.FillCircle(IColor(35, 0, 0, 0), cx + 0.8f, cy + radius * 0.08f, radius + 1.0f);

    // Multi-layer machined rim. The standard IVKnobControl remains visible in
    // the centre, so its pointer/value animation is never replaced or faked.
    g.DrawCircle(IColor(225, 244, 234, 202), cx - 0.20f, cy - 0.35f, radius + 0.25f, nullptr, 1.15f);
    g.DrawCircle(IColor(150, 83, 78, 66), cx + 0.15f, cy + 0.20f, radius - 1.0f, nullptr, 1.25f);
    g.DrawCircle(IColor(130, 201, 83, 43), cx, cy, radius - 2.25f, nullptr, 0.90f);
    g.DrawCircle(IColor(100, 255, 249, 228), cx - 0.20f, cy - 0.30f, radius - 3.4f, nullptr, 0.65f);

    // Specular glints concentrated toward the virtual light source.
    const int glossAlpha = static_cast<int>(std::clamp(92.0f + brightnessBias * 70.0f, 60.0f, 130.0f));
    g.FillCircle(IColor(glossAlpha, 255, 252, 237),
                 cx - radius * 0.36f, cy - radius * 0.38f, radius * 0.090f);
    g.FillCircle(IColor(glossAlpha / 2, 255, 252, 237),
                 cx - radius * 0.22f, cy - radius * 0.48f, radius * 0.050f);

    // A subtle opposite-side shade adds curvature while keeping the pointer
    // and the actual cream knob face readable.
    g.FillCircle(IColor(14, 0, 0, 0), cx + radius * 0.23f, cy + radius * 0.24f, radius * 0.70f);

    // Brushed-metal highlight and lower-edge occlusion.
    g.DrawLine(IColor(105, 255, 250, 230),
               cx - radius * 0.43f, cy - radius * 0.57f,
               cx + radius * 0.13f, cy - radius * 0.57f, nullptr, 0.85f);
    g.DrawLine(IColor(100, 0, 0, 0),
               cx - radius * 0.32f, cy + radius * 0.60f,
               cx + radius * 0.34f, cy + radius * 0.60f, nullptr, 1.05f);
  }
};
} // namespace

void BuildJazzBeatUI(IGraphics* graphics)
{
  if (!graphics)
    return;

  const IColor background(255, 10, 12, 12);
  const IColor fascia(255, 34, 36, 35);
  const IColor fasciaDark(255, 24, 26, 25);
  const IColor cream(255, 232, 222, 192);
  const IColor creamDim(255, 190, 181, 156);
  const IColor textDim(255, 150, 157, 153);
  const IColor accent(255, 201, 83, 43);
  const IColor divider(255, 77, 75, 66);

  // Use only platform fonts. The same calls are executed for APP and VST3
  // because this whole layout lives in one shared compiled object.
  static constexpr const char* kUIFont = "DTJB_UI";
  static constexpr const char* kUIFontBold = "DTJB_UI_BOLD";
  const bool normalFontLoaded = graphics->LoadFont(kUIFont, "Segoe UI", ETextStyle::Normal);
  const bool boldFontLoaded = graphics->LoadFont(kUIFontBold, "Segoe UI", ETextStyle::Bold);
  if (!normalFontLoaded)
    graphics->LoadFont(kUIFont, "Arial", ETextStyle::Normal);
  if (!boldFontLoaded)
    graphics->LoadFont(kUIFontBold, "Arial", ETextStyle::Bold);

  graphics->AttachPanelBackground(background);
  graphics->EnableMouseOver(true);

  const IRECT bounds = graphics->GetBounds();

  graphics->AttachControl(new JazzBeat3DChassisControl(bounds));

  const IText titleText(28.0f, cream, kUIFontBold, EAlign::Center);
  const IText sectionText(11.0f, creamDim, kUIFontBold, EAlign::Center);
  const IText controlText(12.0f, cream, kUIFontBold, EAlign::Center);
  const IText smallText(10.5f, textDim, kUIFont, EAlign::Center);

  graphics->AttachControl(new ITextControl(
    IRECT(24.0f, 14.0f, bounds.R - 24.0f, 49.0f),
    "DER TONDEHR  /  JAZZ BEAT", titleText));

  const IVStyle panelStyle = DEFAULT_STYLE
    .WithColor(kBG, IColor(0, 0, 0, 0))
    .WithColor(kFG, fasciaDark)
    .WithColor(kFR, divider)
    .WithDrawShadows(false)
    .WithShowLabel(false)
    .WithShowValue(false);

  const IVStyle knobStyle = DEFAULT_STYLE
    .WithColor(kBG, fasciaDark)
    .WithColor(kFG, cream)
    .WithColor(kPR, accent)
    .WithColor(kFR, IColor(255, 250, 243, 220))
    .WithColor(kX1, accent)
    .WithDrawShadows(true)
    .WithShowLabel(false)
    .WithShowValue(false);

  const IVStyle switchStyle = DEFAULT_STYLE
    .WithColor(kBG, IColor(255, 38, 40, 39))
    .WithColor(kFG, cream)
    .WithColor(kPR, accent)
    .WithColor(kFR, IColor(255, 225, 216, 186))
    .WithLabelText(IText(10.5f, cream, kUIFontBold, EAlign::Center))
    .WithValueText(IText(10.5f, cream, kUIFontBold, EAlign::Center))
    .WithDrawShadows(true)
    .WithShowLabel(false)
    .WithShowValue(true);

  graphics->AttachControl(new IVPanelControl(
    IRECT(20.0f, 80.0f, bounds.R - 20.0f, 430.0f), "", panelStyle));

  constexpr float kLeft = 27.0f;
  constexpr float kPitch = 108.0f;
  constexpr float kCellW = 96.0f;
  constexpr float kKnobSize = 84.0f;
  constexpr float kSectionTop = 92.0f;
  constexpr float kSectionBottom = 111.0f;
  constexpr float kLabelTop = 121.0f;
  constexpr float kLabelBottom = 143.0f;
  constexpr float kKnobTop = 149.0f;
  constexpr float kValueTop = 249.0f;
  constexpr float kValueBottom = 270.0f;

  auto columnX = [&](int column) {
    return kLeft + static_cast<float>(column) * kPitch;
  };

  auto addText = [&](const IRECT& rect, const char* text, const IText& style) {
    graphics->AttachControl(new ITextControl(rect, text, style));
  };

  auto addSection = [&](int firstColumn, int columns, const char* text) {
    const float left = columnX(firstColumn);
    const float right = columnX(firstColumn + columns - 1) + kCellW;
    addText(IRECT(left, kSectionTop, right, kSectionBottom), text, sectionText);
  };

  addSection(0, 1, "INPUT");
  addSection(1, 1, "DRIVE");
  addSection(2, 4, "AMPLIFIER / EQUALIZER");
  addSection(6, 1, "REVERB");
  addSection(7, 2, "CHORUS");
  addSection(9, 1, "OUTPUT");

  auto addDivider = [&](float x) {
    graphics->AttachControl(new ILambdaControl(
      IRECT(x, 91.0f, x + 2.0f, 396.0f),
      [divider](ILambdaControl*, IGraphics& g, IRECT& r) {
        g.FillRect(IColor(90, 0, 0, 0), r.GetTranslated(1.f, 0.f));
        g.FillRect(IColor(100, divider.R + 25, divider.G + 22, divider.B + 18), IRECT(r.L, r.T, r.L + 1.f, r.B));
      }));
  };

  addDivider(columnX(1) - 8.0f);
  addDivider(columnX(2) - 8.0f);
  addDivider(columnX(6) - 8.0f);
  addDivider(columnX(7) - 8.0f);
  addDivider(columnX(9) - 8.0f);

  auto addKnob = [&](int column, int param, const char* label) {
    const float left = columnX(column);
    addText(IRECT(left, kLabelTop, left + kCellW, kLabelBottom), label, controlText);

    const float knobLeft = left + (kCellW - kKnobSize) * 0.5f;
    const IRECT knobRect(knobLeft, kKnobTop, knobLeft + kKnobSize, kKnobTop + kKnobSize);
    auto* knob = new IVKnobControl(knobRect, param, "", knobStyle, true, false);
    knob->SetTooltip(label);
    graphics->AttachControl(knob);

    graphics->AttachControl(new ICaptionControl(
      IRECT(left, kValueTop, left + kCellW, kValueBottom),
      param,
      IText(10.5f, creamDim, kUIFont, EAlign::Center),
      IColor(0, 0, 0, 0),
      false));
  };

  addKnob(0, kInputTrim, "INPUT TRIM");
  addKnob(1, kDistortion, "DISTORTION");
  addKnob(2, kVolume, "VOLUME");
  addKnob(3, kBass, "BASS");
  addKnob(4, kMiddle, "MIDDLE");
  addKnob(5, kTreble, "TREBLE");
  addKnob(6, kReverb, "REVERB");
  addKnob(7, kChorusRate, "RATE");
  addKnob(8, kChorusDepth, "DEPTH");
  addKnob(9, kOutputTrim, "OUTPUT TRIM");

  constexpr float kSwitchLabelTop = 294.0f;
  constexpr float kSwitchLabelBottom = 315.0f;
  constexpr float kSwitchTop = 321.0f;
  constexpr float kSwitchBottom = 365.0f;

  auto addTabSwitch = [&](float left, float right, int param,
                          const std::vector<const char*>& options,
                          const char* label) {
    addText(IRECT(left, kSwitchLabelTop, right, kSwitchLabelBottom), label, smallText);
    auto* control = new IVTabSwitchControl(
      IRECT(left, kSwitchTop, right, kSwitchBottom),
      param, options, "", switchStyle, EVShape::Rectangle, EDirection::Horizontal);
    control->SetTooltip(label);
    graphics->AttachControl(control);
  };

  addTabSwitch(columnX(0), columnX(0) + kCellW,
               kInputMode, {"HIGH", "LOW"}, "INPUT JACK");

  addTabSwitch(columnX(1), columnX(1) + kCellW,
               kDistortionOn, {"OFF", "ON"}, "DISTORTION");

  const float osLeft = columnX(2);
  const float osRight = columnX(3) + kCellW;
  addTabSwitch(osLeft, osRight,
               kOversampling, {"x1", "x2", "x4"}, "LOCAL OVERSAMPLING");

  // The 1984 service schematic uses VR3 1 M B as a continuously variable
  // HI-TREBLE control, not an on/off switch. Keep it in the same lower-panel
  // location so the rest of the Standalone/VST3 layout is unchanged.
  {
    const float left = columnX(4);
    addText(IRECT(left, 286.0f, left + kCellW, 305.0f), "HI-TREBLE", smallText);
    constexpr float kSmallKnobSize = 54.0f;
    const float knobLeft = left + (kCellW - kSmallKnobSize) * 0.5f;
    auto* knob = new IVKnobControl(
      IRECT(knobLeft, 307.0f, knobLeft + kSmallKnobSize, 307.0f + kSmallKnobSize),
      kHiTreble, "", knobStyle, true, false);
    knob->SetTooltip("HI-TREBLE");
    graphics->AttachControl(knob);
    graphics->AttachControl(new ICaptionControl(
      IRECT(left, 364.0f, left + kCellW, 383.0f),
      kHiTreble,
      IText(10.0f, creamDim, kUIFont, EAlign::Center),
      IColor(0, 0, 0, 0),
      false));
  }

  addTabSwitch(columnX(7), columnX(8) + kCellW,
               kChorusMode, {"OFF", "FIXED", "MANUAL"}, "CHORUS MODE");

  // Draw material/shadow enhancements for the knobs above the standard controls.
  graphics->AttachControl(new JazzBeatKnob3DOverlayControl(bounds));
}
} // namespace dtjb::ui
