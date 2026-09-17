#include "DerTondehrJazzBeat.h"
#include "IPlug_include_in_plug_src.h"
#include "ui/JazzBeatUI.h"
#include "ui/UIScaling.h"

#include <algorithm>
#include <cmath>

#if defined(_MSC_VER)
  #include <xmmintrin.h>
#endif

#if defined(OS_WIN)
  #include <windows.h>
#endif

#if IPLUG_EDITOR
namespace
{
float GetInitialUIScaleForActiveMonitor()
{
#if defined(OS_WIN)
  HWND referenceWindow = GetForegroundWindow();
  HMONITOR monitor = referenceWindow
    ? MonitorFromWindow(referenceWindow, MONITOR_DEFAULTTONEAREST)
    : nullptr;

  if (!monitor)
  {
    POINT cursor{};
    if (GetCursorPos(&cursor))
      monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY);
  }

  MONITORINFO monitorInfo{};
  monitorInfo.cbSize = sizeof(monitorInfo);
  if (monitor && GetMonitorInfoW(monitor, &monitorInfo))
  {
    float dpiScale = 1.0f;
    using GetDpiForMonitorFn = HRESULT (WINAPI*)(HMONITOR, int, UINT*, UINT*);
    if (HMODULE shcore = LoadLibraryW(L"Shcore.dll"))
    {
      const auto getDpiForMonitor = reinterpret_cast<GetDpiForMonitorFn>(
        GetProcAddress(shcore, "GetDpiForMonitor"));
      if (getDpiForMonitor)
      {
        UINT dpiX = 96;
        UINT dpiY = 96;
        if (SUCCEEDED(getDpiForMonitor(monitor, 0, &dpiX, &dpiY)) && dpiX > 0)
          dpiScale = static_cast<float>(dpiX) / 96.0f;
      }
      FreeLibrary(shcore);
    }

    return dtjb::ui::CalculateMonitorFitScale(
      static_cast<float>(monitorInfo.rcWork.right - monitorInfo.rcWork.left),
      static_cast<float>(monitorInfo.rcWork.bottom - monitorInfo.rcWork.top),
      dpiScale);
  }
#endif

  return GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT);
}
}
#endif

#if defined(VST3_API)
  #if !defined(IPLUG_EDITOR) || (IPLUG_EDITOR != 1)
    #error "Der Tondehr Jazz Beat VST3 must be compiled with IPLUG_EDITOR=1"
  #endif
  static_assert(PLUG_HAS_UI == 1, "VST3 custom UI must be enabled");
  static_assert(PLUG_WIDTH == 1140, "Unexpected VST3 editor width");
  static_assert(PLUG_HEIGHT == 510, "Unexpected VST3 editor height");
#endif

DerTondehrJazzBeat::DerTondehrJazzBeat(const InstanceInfo& info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  GetParam(kInputMode)->InitEnum("Input", 0, {"High", "Low"});
  GetParam(kInputTrim)->InitDouble("Input Trim", 0.0, -24.0, 24.0, 0.1, "dB");
  GetParam(kDistortionOn)->InitBool("Distortion", false);
  GetParam(kDistortion)->InitDouble("Distortion Amount", 0.0, 0.0, 100.0, 0.1, "%");
  GetParam(kBass)->InitDouble("Bass", 5.0, 0.0, 10.0, 0.01, "");
  GetParam(kMiddle)->InitDouble("Middle", 5.0, 0.0, 10.0, 0.01, "");
  GetParam(kTreble)->InitDouble("Treble", 5.0, 0.0, 10.0, 0.01, "");
  // ID 7 used to be the public boolean Hi-Treble switch. Preserve its type
  // and ID for VST3 compatibility, but do not expose it in the custom UI.
  GetParam(kHiTrebleLegacy)->InitBool("Hi-Treble (Legacy)", false, "", IParam::kFlagCannotAutomate);
  GetParam(kVolume)->InitDouble("Volume", 5.5, 0.0, 10.0, 0.01, "");
  GetParam(kReverb)->InitDouble("Reverb", 1.8, 0.0, 10.0, 0.01, "");
  GetParam(kChorusMode)->InitEnum("Chorus", 0, {"Off", "Fixed", "Manual"});
  GetParam(kChorusRate)->InitDouble("Chorus Rate", 35.0, 0.0, 100.0, 0.1, "%");
  GetParam(kChorusDepth)->InitDouble("Chorus Depth", 62.0, 0.0, 100.0, 0.1, "%");
  GetParam(kOversampling)->InitEnum("Oversampling", 1, {"x1", "x2", "x4"});
  GetParam(kOutputTrim)->InitDouble("Output Trim", 0.0, -24.0, 12.0, 0.1, "dB");
  // The real continuous VR3 control gets a fresh VST3 ParamID instead of
  // changing the type of the old boolean parameter in-place.
  GetParam(kHiTreble)->InitDouble("Hi-Treble", 0.0, 0.0, 10.0, 0.01, "");

#if IPLUG_EDITOR
  mInitialUIScale = GetInitialUIScaleForActiveMonitor();
  SetEditorSize(static_cast<int>(std::lround(static_cast<float>(PLUG_WIDTH) * mInitialUIScale)),
                static_cast<int>(std::lround(static_cast<float>(PLUG_HEIGHT) * mInitialUIScale)));
#endif
}

void DerTondehrJazzBeat::OnParamChange(int paramIdx, EParamSource source, int sampleOffset)
{
  (void) sampleOffset;

  // Migration path for sessions saved with <= 0.1.13. The old boolean lived
  // at VST3 ParamID 7. If a host restores that legacy value, map OFF/ON to
  // 0/10 on the new continuous VR3 parameter. Newer sessions also restore the
  // fresh continuous ParamID afterwards, so their exact value wins.
  if(paramIdx == kHiTrebleLegacy &&
     (source == kHost || source == kPresetRecall))
  {
    GetParam(kHiTreble)->SetNormalized(GetParam(kHiTrebleLegacy)->Bool() ? 1.0 : 0.0);
  }
}

#if IPLUG_EDITOR
IGraphics* DerTondehrJazzBeat::CreateGraphics()
{
  return MakeGraphics(*this,
                      PLUG_WIDTH,
                      PLUG_HEIGHT,
                      PLUG_FPS,
                      mInitialUIScale);
}

void DerTondehrJazzBeat::LayoutUI(IGraphics* graphics)
{
  // APP and VST3 call the same UI builder from the common source list.
  dtjb::ui::BuildJazzBeatUI(graphics);
}
#endif

#if IPLUG_DSP
void DerTondehrJazzBeat::OnReset()
{
  mSignalChain.prepare(GetSampleRate());

  // VST3 hosts may stop/restart processing at transport boundaries. Start the
  // next processed block with a very short fade rather than exposing a hard
  // discontinuity from zeroed filter/reverb states to an arbitrary waveform
  // sample. Timeline seeks that occur while already playing are detected in
  // ProcessBlock from GetSamplePos().
  mWasTransportRunning = false;
  mHavePreviousBlockPosition = false;
  mTransportPositionTrackingArmed = false;
  mForceFadeIn = true;
  mDeClickRemaining = 0;
  mDeClickLength = 0;
  mDeClickStartLeft = 0.0;
  mDeClickStartRight = 0.0;
  mLastOutputLeft = 0.0;
  mLastOutputRight = 0.0;
}

void DerTondehrJazzBeat::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
#if defined(_MSC_VER)
  _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
#endif

  dtjb::dsp::Parameters parameters;
  parameters.lowInput = GetParam(kInputMode)->Int() == 1;
  parameters.inputTrimDb = GetParam(kInputTrim)->Value();
  parameters.distortionEnabled = GetParam(kDistortionOn)->Bool();
  parameters.distortion = GetParam(kDistortion)->Value() * 0.01;
  parameters.bass = GetParam(kBass)->Value() * 0.1;
  parameters.middle = GetParam(kMiddle)->Value() * 0.1;
  parameters.treble = GetParam(kTreble)->Value() * 0.1;
  parameters.hiTreble = GetParam(kHiTreble)->Value() * 0.1;
  parameters.volume = GetParam(kVolume)->Value() * 0.1;
  parameters.reverb = GetParam(kReverb)->Value() * 0.1;
  parameters.chorusMode = static_cast<dtjb::dsp::BBDChorus::Mode>(GetParam(kChorusMode)->Int());
  parameters.chorusRate = GetParam(kChorusRate)->Value() * 0.01;
  parameters.chorusDepth = GetParam(kChorusDepth)->Value() * 0.01;
  parameters.oversampling = 1 << GetParam(kOversampling)->Int();
  parameters.outputTrimDb = GetParam(kOutputTrim)->Value();
  mSignalChain.setParameters(parameters);

  const bool transportRunning = GetTransportIsRunning();
  const double blockSamplePos = GetSamplePos();
  const bool positionIsFinite = std::isfinite(blockSamplePos);

  bool transportDiscontinuity = false;
  bool preservePreviousOutput = false;

  // A transition from stopped to playing is a known discontinuity. On the
  // first activation (or after OnReset), also fade in even if the host does not
  // provide useful transport state.
  if(mForceFadeIn || (transportRunning && !mWasTransportRunning))
  {
    transportDiscontinuity = true;
    preservePreviousOutput = false;
    mForceFadeIn = false;
  }

  // Once we have observed normal contiguous VST3 sample-position progression,
  // arm seek detection. This avoids false positives in hosts/standalone modes
  // that leave sample position fixed at zero.
  if(transportRunning && mWasTransportRunning && positionIsFinite && mHavePreviousBlockPosition)
  {
    const double expected = mPreviousBlockSamplePos + static_cast<double>(mPreviousBlockFrames);
    const double error = std::abs(blockSamplePos - expected);
    const double tolerance = 2.0;

    if(mTransportPositionTrackingArmed)
    {
      if(error > tolerance)
      {
        transportDiscontinuity = true;
        preservePreviousOutput = true;
      }
    }
    else if(error <= tolerance && mPreviousBlockFrames > 0)
    {
      mTransportPositionTrackingArmed = true;
    }
  }

  if(transportDiscontinuity)
  {
    const double heldLeft = preservePreviousOutput ? mLastOutputLeft : 0.0;
    const double heldRight = preservePreviousOutput ? mLastOutputRight : 0.0;

    // Clear the history-dependent circuit states at the new timeline location.
    // Parameter smoothers keep their current targets, so this does not create
    // a control-value jump.
    mSignalChain.reset();

    mDeClickStartLeft = heldLeft;
    mDeClickStartRight = heldRight;
    mDeClickLength = std::max(16, static_cast<int>(std::lround(GetSampleRate() * 0.005)));
    mDeClickRemaining = mDeClickLength;
  }

  const int inputChannels = NInChansConnected();
  const int outputChannels = NOutChansConnected();
  for(int frame = 0; frame < nFrames; ++frame)
  {
    const double leftIn = inputChannels > 0 ? static_cast<double>(inputs[0][frame]) : 0.0;
    const double rightIn = inputChannels > 1 ? static_cast<double>(inputs[1][frame]) : leftIn;
    auto out = mSignalChain.process(leftIn, rightIn);

    if(mDeClickRemaining > 0 && mDeClickLength > 0)
    {
      const int completed = mDeClickLength - mDeClickRemaining;
      const double phase = static_cast<double>(completed + 1) / static_cast<double>(mDeClickLength);
      const double mix = 0.5 - 0.5 * std::cos(dtjb::dsp::kPi * dtjb::dsp::clamp(phase, 0.0, 1.0));
      out.left = mDeClickStartLeft + (out.left - mDeClickStartLeft) * mix;
      out.right = mDeClickStartRight + (out.right - mDeClickStartRight) * mix;
      --mDeClickRemaining;
    }

    mLastOutputLeft = out.left;
    mLastOutputRight = out.right;

    if(outputChannels > 0) outputs[0][frame] = static_cast<sample>(out.left);
    if(outputChannels > 1) outputs[1][frame] = static_cast<sample>(out.right);
  }

  if(transportRunning && positionIsFinite)
  {
    mPreviousBlockSamplePos = blockSamplePos;
    mPreviousBlockFrames = nFrames;
    mHavePreviousBlockPosition = true;
  }
  else if(!transportRunning)
  {
    mHavePreviousBlockPosition = false;
    mTransportPositionTrackingArmed = false;
  }

  mWasTransportRunning = transportRunning;
}
#endif
