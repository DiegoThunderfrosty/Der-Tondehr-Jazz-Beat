#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "dsp/SignalChain.h"

constexpr int kNumPresets = 1;

#include "Parameters.h"

using namespace iplug;
using namespace igraphics;

class DerTondehrJazzBeat final : public Plugin
{
public:
  explicit DerTondehrJazzBeat(const InstanceInfo& info);

#if IPLUG_EDITOR
  // IMPORTANT: expose the custom editor through virtual overrides instead of
  // constructor-assigned lambdas. Both APP and VST3 therefore enter the exact
  // same CreateGraphics()/LayoutUI() code path when iPlug2 opens the editor.
  IGraphics* CreateGraphics() override;
  void LayoutUI(IGraphics* pGraphics) override;
#endif

#if IPLUG_DSP
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void OnReset() override;
#endif

  // Keep the old VST3 boolean ParamID as a migration-only parameter. The
  // actual continuous VR3 control uses a fresh ParamID so hosts cannot reuse
  // cached boolean metadata for it.
  void OnParamChange(int paramIdx, EParamSource source, int sampleOffset = -1) override;

private:
  dtjb::dsp::SignalChain mSignalChain;

#if IPLUG_DSP
  // Transport de-click state. REAPER/VST3 exposes timeline sample position,
  // which lets us distinguish a genuine seek/play transition from a musical
  // transient. The first few milliseconds after a discontinuity are
  // crossfaded from the previous output instead of allowing a hard step.
  bool mWasTransportRunning = false;
  bool mHavePreviousBlockPosition = false;
  bool mTransportPositionTrackingArmed = false;
  bool mForceFadeIn = true;
  double mPreviousBlockSamplePos = 0.0;
  int mPreviousBlockFrames = 0;
  int mDeClickRemaining = 0;
  int mDeClickLength = 0;
  double mDeClickStartLeft = 0.0;
  double mDeClickStartRight = 0.0;
  double mLastOutputLeft = 0.0;
  double mLastOutputRight = 0.0;
#endif

#if IPLUG_EDITOR
  float mInitialUIScale = 1.0f;
#endif
};
