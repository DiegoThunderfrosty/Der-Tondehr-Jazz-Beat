#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "dsp/SignalChain.h"

#include <atomic>
#include <cstdint>
#include <string>

constexpr int kNumPresets = 1;

#include "Parameters.h"

using namespace iplug;
using namespace igraphics;

class JazzBeatPresetManagerControl;

class DerTondehrJazzBeat final : public Plugin
{
  friend class JazzBeatPresetManagerControl;
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

#if defined(APP_API)
  void SetStandaloneMonoInput(int inputIndex);
  int StandaloneMonoInput() const;
#endif


private:
  static constexpr const char* kPresetExtension = ".dtjbpreset";
  static constexpr int kPresetMaxEntriesPerFolder = 4096;
  static constexpr int kPresetManagerTag = 17001;
  static constexpr int kStandaloneMonoInputTag = 17002;

  bool SavePresetFile(const std::string& path);
  bool LoadPresetFile(const std::string& path);
  bool DeleteSelectedPresetFile();
  bool PresetRootAvailable() const;
  bool PresetPathIsInsideRoot(const std::string& path) const;
  std::string PresetPreferencesPath() const;
  void LoadPresetDirectoryPreference();
  bool SavePresetDirectoryPreferenceValue(const std::string& root) const;
  bool ComputePresetFileIdentity(const std::string& path, std::uint64_t& identity) const;
  void NotifyHostCustomPresetStateChanged();
#if IPLUG_EDITOR
  void PromptSetPresetDirectory();
  void RemovePresetDirectory();
  void PromptSavePreset();
  void MarkPresetUIChanged();
  void PollPresetDirectoryChanges();
  std::uint64_t ComputeCurrentPresetDirectoryFingerprint() const;
  bool ArmSelectedPresetDelete();
  void ClearPresetDeleteIdentity();
#endif

  std::string mPresetRootDirectory;
  std::string mPresetCurrentDirectory;
  std::string mPresetSelectedPath;
  std::string mPresetStatusMessage;
  bool mPresetStatusIsError = false;
  std::atomic<unsigned int> mPresetUIRevision {0};
  std::uint64_t mPresetDirectoryFingerprint = 0u;
  bool mPresetDirectoryFingerprintValid = false;
  std::int64_t mPresetLastDirectoryPollMs = 0;
  bool mPresetBrowserOpen = false;
  std::string mPresetPendingOverwritePath;
  std::uint64_t mPresetPendingOverwriteIdentity = 0u;
  bool mPresetPendingOverwriteValid = false;
  std::uint64_t mPresetDeleteIdentity = 0u;
  bool mPresetDeleteIdentityValid = false;
  std::atomic<bool> mPresetRecallInProgress {false};
  std::atomic<int> mPresetAudioBlocksInFlight {0};

#if IPLUG_DSP
  dtjb::dsp::Parameters ReadParameters();
#endif
#if defined(APP_API)
  void LoadStandaloneAudioPreferences();
  void SaveStandaloneAudioPreferences() const;
#endif

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
#if defined(APP_API)
  // APP captures a contiguous stereo hardware pair. MONO IN chooses which
  // member feeds the actual mono amplifier front-end, with an ~8 ms crossfade.
  std::atomic<int> mStandaloneMonoInputTarget {0};
  double mStandaloneMonoInputBlend = 0.0;
#endif
#endif

#if IPLUG_EDITOR
  float mInitialUIScale = 1.0f;
#endif
};
