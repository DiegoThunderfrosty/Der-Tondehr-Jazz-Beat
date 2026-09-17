#include "JazzBeatUI.h"
#include "../Parameters.h"
#include "IControls.h"

using namespace iplug;
using namespace igraphics;

namespace dtjb::ui
{
void BuildJazzBeatUI(IGraphics* graphics)
{
  if (!graphics)
    return;

  const IColor background(255, 18, 20, 20);
  const IColor fascia(255, 34, 36, 35);
  const IColor fasciaDark(255, 27, 29, 28);
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

  const IText titleText(28.0f, cream, kUIFontBold, EAlign::Center);
  const IText sectionText(11.0f, creamDim, kUIFontBold, EAlign::Center);
  const IText controlText(12.0f, cream, kUIFontBold, EAlign::Center);
  const IText smallText(10.5f, textDim, kUIFont, EAlign::Center);

  graphics->AttachControl(new ITextControl(
    IRECT(24.0f, 14.0f, bounds.R - 24.0f, 49.0f),
    "DER TONDEHR  /  JAZZ BEAT", titleText));

  const IVStyle panelStyle = DEFAULT_STYLE
    .WithColor(kBG, fascia)
    .WithColor(kFG, fasciaDark)
    .WithColor(kFR, divider)
    .WithDrawShadows(false)
    .WithShowLabel(false)
    .WithShowValue(false);

  const IVStyle knobStyle = DEFAULT_STYLE
    .WithColor(kBG, fasciaDark)
    .WithColor(kFG, cream)
    .WithColor(kPR, accent)
    .WithColor(kFR, creamDim)
    .WithColor(kX1, accent)
    .WithDrawShadows(false)
    .WithShowLabel(false)
    .WithShowValue(false);

  const IVStyle switchStyle = DEFAULT_STYLE
    .WithColor(kBG, IColor(255, 32, 34, 33))
    .WithColor(kFG, cream)
    .WithColor(kPR, accent)
    .WithColor(kFR, creamDim)
    .WithLabelText(IText(10.5f, cream, kUIFontBold, EAlign::Center))
    .WithValueText(IText(10.5f, cream, kUIFontBold, EAlign::Center))
    .WithDrawShadows(false)
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
      IRECT(x, 91.0f, x + 1.0f, 396.0f),
      [divider](ILambdaControl*, IGraphics& g, IRECT& r) {
        g.FillRect(divider, r);
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
      fascia,
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
      fascia,
      false));
  }

  addTabSwitch(columnX(7), columnX(8) + kCellW,
               kChorusMode, {"OFF", "FIXED", "MANUAL"}, "CHORUS MODE");

}
} // namespace dtjb::ui
