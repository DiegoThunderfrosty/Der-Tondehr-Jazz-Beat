#pragma once

enum EParams
{
  kInputMode = 0,
  kInputTrim,
  kDistortionOn,
  kDistortion,
  kBass,
  kMiddle,
  kTreble,

  // VST3 PARAMETER-COMPATIBILITY NOTE:
  // ID 7 shipped as a boolean Hi-Treble switch in <= 0.1.13. Do not reuse
  // that VST3 ParamID for the later continuous potentiometer. Hosts are
  // allowed to retain parameter metadata/state by ParamID. Keep ID 7 as a
  // legacy bool and put the real continuous Hi-Treble at a fresh ID below.
  kHiTrebleLegacy,

  kVolume,
  kReverb,
  kChorusMode,
  kChorusRate,
  kChorusDepth,
  kOversampling,
  kOutputTrim,

  // Continuous VR3 1 M B Hi-Treble control introduced after the old bool.
  // It intentionally has a NEW VST3 ParamID.
  kHiTreble,

  kNumParams
};

static_assert(kHiTrebleLegacy == 7, "Legacy VST3 Hi-Treble ParamID must remain 7");
static_assert(kVolume == 8, "Existing VST3 ParamIDs must not move");
static_assert(kOutputTrim == 14, "Existing VST3 ParamIDs must not move");
static_assert(kHiTreble == 15, "Continuous Hi-Treble must use a fresh VST3 ParamID");
