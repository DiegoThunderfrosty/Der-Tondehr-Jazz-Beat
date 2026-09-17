cmake_minimum_required(VERSION 3.24)

project(DerTondehrJazzBeat VERSION 0.1.28 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(IPLUG2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/iPlug2" CACHE PATH "iPlug2 root")
# Build artifacts stay in build/out. Deployment is intentionally disabled:
# the developer installs/replaces the VST3 manually.
set(IPLUG_DEPLOY_PLUGINS OFF CACHE BOOL "Do not auto-deploy plug-ins" FORCE)

if(NOT EXISTS "${IPLUG2_DIR}/iPlug2.cmake")
  message(FATAL_ERROR "iPlug2 is missing. Clone it to external/iPlug2.")
endif()

foreach(_sdk_part base pluginterfaces public.sdk)
  if(NOT EXISTS "${IPLUG2_DIR}/Dependencies/IPlug/VST3_SDK/${_sdk_part}")
    message(FATAL_ERROR
      "VST3 SDK part '${_sdk_part}' is missing. Run scripts/link-vst3-sdk.ps1 after cloning external/vst3sdk.")
  endif()
endforeach()

include("${IPLUG2_DIR}/iPlug2.cmake")
find_package(iPlug2 REQUIRED)

# IMPORTANT:
# The UI implementation is listed directly in the common plugin source list.
# iPlug2's iplug_add_plugin() then adds this exact source file to BOTH the
# VST3 and APP targets. Each target compiles it in its own API context while
# inheriting the correct IGraphics backend definitions/include paths from
# iPlug2. Do not split the UI into a separate OBJECT target: doing so bypasses
# iPlug2's transitive graphics usage requirements and caused the previous
# nanovg.h / Easing.h build failures.
set(DTJB_SOURCES
  src/DerTondehrJazzBeat.cpp
  src/DerTondehrJazzBeat.h
  src/Parameters.h
  src/preset/JazzBeatPresetFormat.h
  src/preset/JazzBeatPresetMap.h
  src/ui/JazzBeatUI.cpp
  src/ui/JazzBeatUI.h
  src/ui/UIScaling.h
  src/dsp/DspCommon.h
  src/dsp/InputAndPreamp.h
  src/dsp/DistortionStage.h
  src/dsp/ToneAndVolume.h
  src/dsp/SpringReverb.h
  src/dsp/BBDChorus.h
  src/dsp/PowerAmp.h
  src/dsp/SignalChain.h
  resources/resource.h
)

iplug_add_plugin(${PROJECT_NAME}
  SOURCES ${DTJB_SOURCES}
  FORMATS VST3 APP
  UI IGRAPHICS
)

if(NOT TARGET ${PROJECT_NAME}-vst3)
  message(FATAL_ERROR "Expected target ${PROJECT_NAME}-vst3 was not created")
endif()
if(NOT TARGET ${PROJECT_NAME}-app)
  message(FATAL_ERROR "Expected target ${PROJECT_NAME}-app was not created")
endif()

# iPlug2::VST3 and iPlug2::APP already supply IPLUG_EDITOR/IPLUG_DSP and the
# selected IGraphics backend. The compile-time checks in DerTondehrJazzBeat.cpp
# still guarantee that the VST3 cannot be built without the custom editor.

if(MSVC)
  # RtAudio/RtMidi use exceptions for device errors in the standalone target.
  target_compile_options(DerTondehrJazzBeat-app PRIVATE /EHsc)
endif()

# -----------------------------------------------------------------------------
# Der Tondehr Jazz Beat standalone audio/driver system (Windows APP only)
# -----------------------------------------------------------------------------
# Keep the standalone stream-opening path as close as possible to stock iPlug2.
# We only add WASAPI, bounded shutdown, the upstream ASIO input-list correction,
# and visible release-build diagnostics. The RtAudio stream now honors the
# stereo input/output pairs selected in Preferences. This matters especially for
# ASIO, where one device exposes all hardware channels: selecting pair 3/4 must
# open firstChannel=2, not the hard-coded 1/2 pair. MONO IN 1/2 then chooses
# which member of that captured pair feeds Jazz Beat's mono front-end. The user's external iPlug2
# checkout remains untouched, and VST3 never links any RtAudio/WASAPI code.
if(WIN32 AND TARGET iPlug2::APP)
  set(_jb_app_host_src "${IPLUG2_DIR}/IPlug/APP/IPlugAPP_host.cpp")
  set(_jb_app_dialog_src "${IPLUG2_DIR}/IPlug/APP/IPlugAPP_dialog.cpp")
  if(NOT EXISTS "${_jb_app_host_src}" OR NOT EXISTS "${_jb_app_dialog_src}")
    message(FATAL_ERROR "JazzBeat: required iPlug2 APP sources were not found")
  endif()

  file(READ "${_jb_app_host_src}" _jb_host)
  file(READ "${_jb_app_dialog_src}" _jb_dialog)
  set(_jb_host_original "${_jb_host}")
  set(_jb_dialog_original "${_jb_dialog}")

  string(REPLACE "using namespace iplug;\n" "using namespace iplug;\n\n#ifdef OS_WIN\nstatic constexpr int kJazzBeatDeviceWASAPI = 2;\n#endif\n" _jb_host "${_jb_host}")
  string(REPLACE
    "if (mState.mAudioDriverType == kDeviceASIO)\n    mDAC = std::make_unique<RtAudio>(RtAudio::WINDOWS_ASIO);\n  else if (mState.mAudioDriverType == kDeviceDS)\n    mDAC = std::make_unique<RtAudio>(RtAudio::WINDOWS_DS);"
    "if (mState.mAudioDriverType == kDeviceASIO)\n    mDAC = std::make_unique<RtAudio>(RtAudio::WINDOWS_ASIO);\n  else if (mState.mAudioDriverType == kJazzBeatDeviceWASAPI)\n    mDAC = std::make_unique<RtAudio>(RtAudio::WINDOWS_WASAPI);\n  else\n  {\n    mState.mAudioDriverType = kDeviceDS;\n    mDAC = std::make_unique<RtAudio>(RtAudio::WINDOWS_DS);\n  }"
    _jb_host "${_jb_host}")
  # Stock iPlug2 currently hard-codes firstChannel=0 even though its Preferences
  # dialog stores mAudioInChanL/mAudioOutChanL. DirectSound/WASAPI often expose
  # hardware pairs as separate endpoints, masking this bug; ASIO exposes the whole
  # interface as one device, so choosing 3/4 still captured 1/2. Honor the selected
  # physical contiguous stereo pair for every backend. RtAudio uses zero-based
  # firstChannel. Jazz Beat then chooses one member with its APP-only MONO IN control.
  string(REPLACE
    "  iParams.nChannels = GetPlug()->MaxNChannels(ERoute::kInput); // TODO: flexible channel count\n  iParams.firstChannel = 0; // TODO: flexible channel count\n\n  oParams.deviceId = outID;\n  oParams.nChannels = GetPlug()->MaxNChannels(ERoute::kOutput); // TODO: flexible channel count\n  oParams.firstChannel = 0; // TODO: flexible channel count"
    "  iParams.nChannels = GetPlug()->MaxNChannels(ERoute::kInput); // stereo pair exposed to the APP target\n  iParams.firstChannel = (mState.mAudioInChanL > 0) ? (mState.mAudioInChanL - 1) : 0;\n\n  oParams.deviceId = outID;\n  oParams.nChannels = GetPlug()->MaxNChannels(ERoute::kOutput); // stereo pair exposed by the plug\n  oParams.firstChannel = (mState.mAudioOutChanL > 0) ? (mState.mAudioOutChanL - 1) : 0;"
    _jb_host "${_jb_host}")

  string(REPLACE
    "      while (!mAudioDone)\n        Sleep(10);\n      \n      mDAC->abortStream();"
    "      // Never wait forever for a dead/unplugged driver to deliver another callback.\n      for (int wait = 0; wait < 50 && !mAudioDone; ++wait)\n        Sleep(10);\n      if (mDAC->isStreamRunning())\n        mDAC->abortStream();\n      mAudioDone = true;"
    _jb_host "${_jb_host}")

  # Jazz Beat always produces two channels, so do not offer output devices that
  # cannot physically open the required stereo stream. Mono/one-channel input
  # devices remain valid because the plug consumes exactly one input channel.
  string(REPLACE
    "    if (info.outputChannels > 0)\n    {\n      mAudioOutputDevIDs.push_back(deviceID);\n    }"
    "    if (info.outputChannels >= static_cast<unsigned int>(GetPlug()->MaxNChannels(ERoute::kOutput)))\n    {\n      mAudioOutputDevIDs.push_back(deviceID);\n    }"
    _jb_host "${_jb_host}")
  string(REPLACE
    "    if (info.isDefaultOutput)\n    {\n      mDefaultOutputDev = deviceID;\n    }"
    "    if (info.isDefaultOutput && info.outputChannels >= static_cast<unsigned int>(GetPlug()->MaxNChannels(ERoute::kOutput)))\n    {\n      mDefaultOutputDev = deviceID;\n    }"
    _jb_host "${_jb_host}")

  # Reset per-backend default-device IDs every time devices are probed. Without
  # this, a default ID from WASAPI/DirectSound can survive an API switch and be
  # mistaken for a valid default in the newly selected backend.
  string(REPLACE
    "void IPlugAPPHost::ProbeAudioIO()\n{\n  mAudioInputDevIDs.clear();\n  mAudioOutputDevIDs.clear();"
    "void IPlugAPPHost::ProbeAudioIO()\n{\n  mAudioInputDevIDs.clear();\n  mAudioOutputDevIDs.clear();\n  mDefaultInputDev.reset();\n  mDefaultOutputDev.reset();"
    _jb_host "${_jb_host}")

  # DirectSound is RtAudio's compatibility backend on Windows and its default
  # zero-valued StreamOptions::numberOfBuffers chooses a conservative multi-buffer
  # configuration. That is robust, but it adds a very noticeable monitoring delay
  # for an amp simulator. Keep the user-selected bufferFrames value, but force the
  # DirectSound backend to use RtAudio's low-latency path and the minimum practical
  # two-buffer queue. WASAPI and ASIO are intentionally left unchanged.
  string(REPLACE
    "  RtAudio::StreamOptions options;\n  options.flags = RTAUDIO_NONINTERLEAVED;"
    "  RtAudio::StreamOptions options;\n  options.flags = RTAUDIO_NONINTERLEAVED;\n#ifdef OS_WIN\n  if (mState.mAudioDriverType == kDeviceDS)\n  {\n    options.flags |= RTAUDIO_MINIMIZE_LATENCY;\n    options.numberOfBuffers = 2;\n  }\n#endif"
    _jb_host "${_jb_host}")

  # RtAudio/ASIO can clamp a requested buffer size to a driver-supported value.
  # Persist the actual value returned by openStream so the Preferences UI and
  # the next restart do not keep requesting a stale/unsupported size.
  string(REPLACE
    [=[  auto status = mDAC->openStream(&oParams, iParams.nChannels > 0 ? &iParams : nullptr, RTAUDIO_FLOAT64, sr, &mBufferSize, &AudioCallback, this, &options);

  if (status != RtAudioErrorType::RTAUDIO_NO_ERROR)]=]
    [=[  auto status = mDAC->openStream(&oParams, iParams.nChannels > 0 ? &iParams : nullptr, RTAUDIO_FLOAT64, sr, &mBufferSize, &AudioCallback, this, &options);

  if (status == RtAudioErrorType::RTAUDIO_NO_ERROR)
    mState.mBufferSize = mBufferSize;

  if (status != RtAudioErrorType::RTAUDIO_NO_ERROR)]=]
    _jb_host "${_jb_host}")

  # Release APPs have no console, so stock DBGMSG/stderr failures are invisible.
  # Surface stream-open/start/backend errors for every API, not just WASAPI.
  string(REPLACE
    "    DBGMSG(\"%s\", mDAC->getErrorText().c_str());\n    return false;"
    "    const std::string jazzbeatError = mDAC->getErrorText();\n    MessageBoxA(gHWND, jazzbeatError.c_str(), \"Der Tondehr Jazz Beat - audio stream open failed\", MB_OK | MB_ICONERROR);\n    return false;"
    _jb_host "${_jb_host}")
  string(REPLACE
    "    DBGMSG(\"Error starting stream: %s\\n\", mDAC->getErrorText().c_str());\n    return false;"
    "    const std::string jazzbeatError = mDAC->getErrorText();\n    MessageBoxA(gHWND, jazzbeatError.c_str(), \"Der Tondehr Jazz Beat - audio stream start failed\", MB_OK | MB_ICONERROR);\n    return false;"
    _jb_host "${_jb_host}")
  string(REPLACE
    "void IPlugAPPHost::ErrorCallback(RtAudioErrorType type, const std::string &errorText)\n{\n  std::cerr << \"\\nerrorCallback: \" << errorText << \"\\n\\n\";\n}"
    "void IPlugAPPHost::ErrorCallback(RtAudioErrorType type, const std::string &errorText)\n{\n  if (type != RTAUDIO_WARNING && type != RTAUDIO_NO_ERROR)\n    MessageBoxA(gHWND, errorText.c_str(), \"Der Tondehr Jazz Beat - audio driver error\", MB_OK | MB_ICONERROR);\n}"
    _jb_host "${_jb_host}")

  # A duplex effect must receive a capture buffer.  If a backend somehow opens
  # output-only, make that failure explicit instead of silently feeding null.
  string(REPLACE
    "  double* pInputBufferD = static_cast<double*>(pInputBuffer);\n  double* pOutputBufferD = static_cast<double*>(pOutputBuffer);"
    "  double* pInputBufferD = static_cast<double*>(pInputBuffer);\n  double* pOutputBufferD = static_cast<double*>(pOutputBuffer);\n  if (nins > 0 && pInputBufferD == nullptr)\n  {\n    static bool jazzbeatNullInputShown = false;\n    if (!jazzbeatNullInputShown)\n    {\n      jazzbeatNullInputShown = true;\n      MessageBoxA(gHWND, \"The audio backend opened without a capture buffer. Try another input device/API and send this message back for diagnosis.\", \"Der Tondehr Jazz Beat - no input buffer\", MB_OK | MB_ICONERROR);\n    }\n    if (pOutputBufferD)\n      memset(pOutputBufferD, 0, nFrames * nouts * sizeof(double));\n    return 0;\n  }"
    _jb_host "${_jb_host}")

  string(REPLACE "#include \"asio.h\"\n" "#include \"asio.h\"\n#include \"asiodrivers.h\"\n#include <algorithm>\n#include <cstdio>\n" _jb_dialog "${_jb_dialog}")

  string(REPLACE "using namespace igraphics;\n" [=[using namespace igraphics;

#ifdef OS_WIN
static constexpr int kJazzBeatDeviceWASAPI = 2;

struct JazzBeatASIOBufferCaps
{
  long minSize = 0;
  long maxSize = 0;
  long preferredSize = 0;
  long granularity = 0;
};

static bool JazzBeatQueryASIOBufferCaps(const char* driverName, bool driverAlreadyLoaded, JazzBeatASIOBufferCaps& caps)
{
  if (!driverName || !*driverName)
    return false;

  if (driverAlreadyLoaded)
  {
    return ASIOGetBufferSize(&caps.minSize, &caps.maxSize, &caps.preferredSize, &caps.granularity) == ASE_OK
      && caps.minSize > 0 && caps.maxSize >= caps.minSize;
  }

  AsioDrivers queryDrivers;
  if (!queryDrivers.loadDriver(const_cast<char*>(driverName)))
    return false;

  ASIODriverInfo info = {};
  info.asioVersion = 2;
  info.sysRef = GetForegroundWindow();
  const ASIOError initResult = ASIOInit(&info);
  bool ok = false;
  if (initResult == ASE_OK)
  {
    ok = ASIOGetBufferSize(&caps.minSize, &caps.maxSize, &caps.preferredSize, &caps.granularity) == ASE_OK
      && caps.minSize > 0 && caps.maxSize >= caps.minSize;
    ASIOExit();
  }
  queryDrivers.removeCurrentDriver();
  return ok;
}

static bool JazzBeatOpenASIOControlPanelStopped(const char* driverName, JazzBeatASIOBufferCaps& caps)
{
  if (!driverName || !*driverName)
    return false;

  AsioDrivers panelDrivers;
  if (!panelDrivers.loadDriver(const_cast<char*>(driverName)))
    return false;

  ASIODriverInfo info = {};
  info.asioVersion = 2;
  info.sysRef = GetForegroundWindow();
  const ASIOError initResult = ASIOInit(&info);
  bool ok = false;
  if (initResult == ASE_OK)
  {
    ASIOControlPanel();
    Sleep(100);
    ok = ASIOGetBufferSize(&caps.minSize, &caps.maxSize, &caps.preferredSize, &caps.granularity) == ASE_OK
      && caps.minSize > 0 && caps.maxSize >= caps.minSize;
    ASIOExit();
  }
  panelDrivers.removeCurrentDriver();
  return ok;
}

static bool JazzBeatASIOBufferAllowed(long value, const JazzBeatASIOBufferCaps& caps)
{
  if (value < caps.minSize || value > caps.maxSize)
    return false;
  if (caps.granularity == -1)
    return value > 0 && (value & (value - 1)) == 0;
  if (caps.granularity > 0)
    return (value % caps.granularity) == 0;
  return true;
}

static uint32_t JazzBeatNormalizeASIOBuffer(uint32_t requested, const JazzBeatASIOBufferCaps& caps)
{
  long value = requested ? static_cast<long>(requested) : caps.preferredSize;
  if (value < caps.minSize) value = caps.minSize;
  if (value > caps.maxSize) value = caps.maxSize;

  if (caps.granularity == -1)
  {
    long best = 0;
    long bestDistance = std::numeric_limits<long>::max();
    for (long candidate = 1; candidate > 0 && candidate <= caps.maxSize; candidate <<= 1)
    {
      if (candidate < caps.minSize)
        continue;
      const long distance = std::labs(candidate - value);
      if (distance < bestDistance)
      {
        best = candidate;
        bestDistance = distance;
      }
      if (candidate > (std::numeric_limits<long>::max() >> 1))
        break;
    }
    if (best > 0) value = best;
  }
  else if (caps.granularity > 0)
  {
    const long first = ((caps.minSize + caps.granularity - 1) / caps.granularity) * caps.granularity;
    const long last = (caps.maxSize / caps.granularity) * caps.granularity;
    if (first <= last)
    {
      long rounded = ((value + caps.granularity / 2) / caps.granularity) * caps.granularity;
      if (rounded < first) rounded = first;
      if (rounded > last) rounded = last;
      value = rounded;
    }
  }

  if (value <= 0)
    value = caps.preferredSize > 0 ? caps.preferredSize : caps.minSize;
  return static_cast<uint32_t>(value);
}
#endif
]=] _jb_dialog "${_jb_dialog}")
  string(REPLACE
    "  SendDlgItemMessage(hwndDlg,IDC_COMBO_AUDIO_DRIVER,CB_ADDSTRING,0,(LPARAM)\"ASIO\");\n  SendDlgItemMessage(hwndDlg,IDC_COMBO_AUDIO_DRIVER,CB_SETCURSEL, mState.mAudioDriverType, 0);"
    "  SendDlgItemMessage(hwndDlg,IDC_COMBO_AUDIO_DRIVER,CB_ADDSTRING,0,(LPARAM)\"ASIO\");\n  SendDlgItemMessage(hwndDlg,IDC_COMBO_AUDIO_DRIVER,CB_ADDSTRING,0,(LPARAM)\"WASAPI\");\n  if (mState.mAudioDriverType < kDeviceDS || mState.mAudioDriverType > kJazzBeatDeviceWASAPI)\n    mState.mAudioDriverType = kDeviceDS;\n  SendDlgItemMessage(hwndDlg,IDC_COMBO_AUDIO_DRIVER,CB_SETCURSEL, mState.mAudioDriverType, 0);"
    _jb_dialog "${_jb_dialog}")
  # Upstream iPlug2 fix: in ASIO the input and output are the same device.
  # Keep the input-list index synchronized with the selected ASIO output device,
  # otherwise channel names/counts can come from an unrelated device.
  string(REPLACE
    "  if (driverType == kDeviceASIO)\n    SendDlgItemMessage(hwndDlg,IDC_COMBO_AUDIO_IN_DEV,CB_SETCURSEL, outdevidx, 0);\n  else\n#endif\n    SendDlgItemMessage(hwndDlg,IDC_COMBO_AUDIO_IN_DEV,CB_SETCURSEL, indevidx, 0);"
    "  if (driverType == kDeviceASIO)\n  {\n    SendDlgItemMessage(hwndDlg,IDC_COMBO_AUDIO_IN_DEV,CB_SETCURSEL, outdevidx, 0);\n    indevidx = outdevidx;\n  }\n  else\n#endif\n    SendDlgItemMessage(hwndDlg,IDC_COMBO_AUDIO_IN_DEV,CB_SETCURSEL, indevidx, 0);"
    _jb_dialog "${_jb_dialog}")

  # Keep one remembered audio setup per Windows backend. These profiles are
  # persisted as extra sections in iPlug2's existing settings.ini and are only
  # committed when Preferences is accepted with OK. Switching APIs inside the
  # dialog therefore restores the last device/channels/rate/buffer used by that
  # API without making Cancel destructive.
  string(REPLACE
    [=[  AppState& mActiveState = _this->mActiveState;

  auto getComboString = [&](WDL_String& str, int item, WPARAM idx) {]=]
    [=[  AppState& mActiveState = _this->mActiveState;

#ifdef OS_WIN
  static AppState driverProfiles[3];
  static bool driverProfileValid[3] = { false, false, false };

  auto driverSection = [](int driver) -> const char* {
    switch (driver)
    {
      case kDeviceASIO: return "audio_profile_asio";
      case kJazzBeatDeviceWASAPI: return "audio_profile_wasapi";
      default: return "audio_profile_directsound";
    }
  };

  auto loadDriverProfile = [&](int driver, AppState& state) -> bool {
    const char* section = driverSection(driver);
    const char* ini = _this->mINIPath.Get();
    if (GetPrivateProfileInt(section, "configured", 0, ini) != 1)
      return false;

    char text[512] = {};
    state = AppState();
    state.mAudioDriverType = static_cast<uint32_t>(driver);
    GetPrivateProfileString(section, "indev", "", text, sizeof(text), ini);
    state.mAudioInDev.Set(text);
    GetPrivateProfileString(section, "outdev", "", text, sizeof(text), ini);
    state.mAudioOutDev.Set(text);
    state.mAudioInChanL = std::max<uint32_t>(1u, static_cast<uint32_t>(GetPrivateProfileInt(section, "in1", 1, ini)));
    state.mAudioInChanR = std::max<uint32_t>(1u, static_cast<uint32_t>(GetPrivateProfileInt(section, "in2", 2, ini)));
    state.mAudioOutChanL = std::max<uint32_t>(1u, static_cast<uint32_t>(GetPrivateProfileInt(section, "out1", 1, ini)));
    state.mAudioOutChanR = std::max<uint32_t>(1u, static_cast<uint32_t>(GetPrivateProfileInt(section, "out2", 2, ini)));
    state.mBufferSize = static_cast<uint32_t>(GetPrivateProfileInt(section, "buffer", 512, ini));
    state.mAudioSR = std::max<uint32_t>(1u, static_cast<uint32_t>(GetPrivateProfileInt(section, "sr", 44100, ini)));
    return true;
  };

  auto saveDriverProfile = [&](const AppState& state) {
    const int driver = static_cast<int>(state.mAudioDriverType);
    if (driver < kDeviceDS || driver > kJazzBeatDeviceWASAPI)
      return;
    const char* section = driverSection(driver);
    const char* ini = _this->mINIPath.Get();
    char value[64] = {};
    WritePrivateProfileString(section, "configured", "1", ini);
    WritePrivateProfileString(section, "indev", state.mAudioInDev.Get(), ini);
    WritePrivateProfileString(section, "outdev", state.mAudioOutDev.Get(), ini);
    sprintf(value, "%u", state.mAudioInChanL); WritePrivateProfileString(section, "in1", value, ini);
    sprintf(value, "%u", state.mAudioInChanR); WritePrivateProfileString(section, "in2", value, ini);
    sprintf(value, "%u", state.mAudioOutChanL); WritePrivateProfileString(section, "out1", value, ini);
    sprintf(value, "%u", state.mAudioOutChanR); WritePrivateProfileString(section, "out2", value, ini);
    sprintf(value, "%u", state.mBufferSize); WritePrivateProfileString(section, "buffer", value, ini);
    sprintf(value, "%u", state.mAudioSR); WritePrivateProfileString(section, "sr", value, ini);
  };

  auto sanitizeAudioStateForCurrentBackend = [&]() {
    auto nameForID = [&](uint32_t id) -> std::string { return _this->GetAudioDeviceName(id); };
    auto hasNamedDevice = [&](const std::vector<uint32_t>& ids, const char* name) -> bool {
      if (!name || !*name) return false;
      for (uint32_t id : ids)
        if (std::string_view(nameForID(id)) == name) return true;
      return false;
    };
    auto chooseFallback = [&](const std::vector<uint32_t>& ids, const std::optional<uint32_t>& defaultID) -> std::string {
      if (defaultID && std::find(ids.begin(), ids.end(), defaultID.value()) != ids.end())
        return nameForID(defaultID.value());
      if (!ids.empty())
        return nameForID(ids.front());
      return std::string();
    };

    if (mState.mAudioDriverType == kDeviceASIO)
    {
      if (!hasNamedDevice(_this->mAudioOutputDevIDs, mState.mAudioOutDev.Get()))
        mState.mAudioOutDev.Set(chooseFallback(_this->mAudioOutputDevIDs, _this->mDefaultOutputDev).c_str());
      mState.mAudioInDev.Set(mState.mAudioOutDev.Get());
    }
    else
    {
      if (!hasNamedDevice(_this->mAudioInputDevIDs, mState.mAudioInDev.Get()))
        mState.mAudioInDev.Set(chooseFallback(_this->mAudioInputDevIDs, _this->mDefaultInputDev).c_str());
      if (!hasNamedDevice(_this->mAudioOutputDevIDs, mState.mAudioOutDev.Get()))
        mState.mAudioOutDev.Set(chooseFallback(_this->mAudioOutputDevIDs, _this->mDefaultOutputDev).c_str());
    }

    const auto inputID = _this->GetAudioDeviceID(mState.mAudioDriverType == kDeviceASIO ? mState.mAudioOutDev.Get() : mState.mAudioInDev.Get());
    const auto outputID = _this->GetAudioDeviceID(mState.mAudioOutDev.Get());
    if (!inputID || !outputID || !_this->mDAC)
      return;

    RtAudio::DeviceInfo inputInfo = _this->mDAC->getDeviceInfo(inputID.value());
    RtAudio::DeviceInfo outputInfo = _this->mDAC->getDeviceInfo(outputID.value());

    auto clampStereoPair = [](uint32_t& left, uint32_t& right, unsigned int channels) {
      if (channels < 2)
      {
        left = 1;
        right = 1;
        return;
      }
      const uint32_t maxLeft = static_cast<uint32_t>(channels - 1);
      left = std::clamp(left, 1u, maxLeft);
      right = left + 1;
    };
    clampStereoPair(mState.mAudioInChanL, mState.mAudioInChanR, inputInfo.inputChannels);
    clampStereoPair(mState.mAudioOutChanL, mState.mAudioOutChanR, outputInfo.outputChannels);

    auto supportsRate = [](const RtAudio::DeviceInfo& info, uint32_t rate) {
      return std::find(info.sampleRates.begin(), info.sampleRates.end(), rate) != info.sampleRates.end();
    };
    if (!supportsRate(inputInfo, mState.mAudioSR) || !supportsRate(outputInfo, mState.mAudioSR))
    {
      uint32_t replacement = 0;
      const uint32_t preferredOut = outputInfo.preferredSampleRate;
      const uint32_t preferredIn = inputInfo.preferredSampleRate;
      if (preferredOut && supportsRate(inputInfo, preferredOut) && supportsRate(outputInfo, preferredOut))
        replacement = preferredOut;
      else if (preferredIn && supportsRate(inputInfo, preferredIn) && supportsRate(outputInfo, preferredIn))
        replacement = preferredIn;
      else if (supportsRate(inputInfo, 48000) && supportsRate(outputInfo, 48000))
        replacement = 48000;
      else if (supportsRate(inputInfo, 44100) && supportsRate(outputInfo, 44100))
        replacement = 44100;
      else
      {
        for (unsigned int rate : outputInfo.sampleRates)
        {
          if (supportsRate(inputInfo, rate))
          {
            replacement = rate;
            break;
          }
        }
      }
      if (replacement) mState.mAudioSR = replacement;
    }

    if (mState.mAudioDriverType == kDeviceASIO)
    {
      JazzBeatASIOBufferCaps caps;
      const bool streamLoaded = _this->mDAC->isStreamOpen();
      if (JazzBeatQueryASIOBufferCaps(outputInfo.name.c_str(), streamLoaded, caps))
        mState.mBufferSize = JazzBeatNormalizeASIOBuffer(mState.mBufferSize, caps);
    }
  };
#endif

  auto getComboString = [&](WDL_String& str, int item, WPARAM idx) {]=]
    _jb_dialog "${_jb_dialog}")

  # Pause the standalone audio stream while Preferences is open. This avoids
  # reconfiguring live DirectSound/WASAPI/ASIO streams and makes vendor ASIO
  # control-panel/buffer changes safer. Apply intentionally starts audio so the
  # user can audition the new settings; any subsequent audio-setting edit stops
  # the stream again before changing state. OK always starts the selected setup,
  # and Cancel always restores/restarts the exact setup that was active when the
  # dialog opened.
  string(REPLACE
    [=[      _this->PopulatePreferencesDialog(hwndDlg);
      mTempState = mState;]=]
    [=[      mTempState = mState;
#ifdef OS_WIN
      for (int driver = kDeviceDS; driver <= kJazzBeatDeviceWASAPI; ++driver)
      {
        driverProfiles[driver] = AppState();
        driverProfiles[driver].mAudioDriverType = static_cast<uint32_t>(driver);
        driverProfileValid[driver] = loadDriverProfile(driver, driverProfiles[driver]);
      }
      if (mState.mAudioDriverType <= kJazzBeatDeviceWASAPI)
      {
        driverProfiles[mState.mAudioDriverType] = mState;
        driverProfileValid[mState.mAudioDriverType] = true;
      }
#endif
      _this->CloseAudio();
      _this->PopulatePreferencesDialog(hwndDlg);]=]
    _jb_dialog "${_jb_dialog}")

  # Fix zero-based CB_GETCURSEL bookkeeping for the right-channel selectors.
  # Both input selectors remain enabled exactly as in Bass Suite/Crunchy.
  # MONO IN 1/2 in the Standalone UI decides which captured channel feeds the
  # mono Jazz Beat DSP. Keep AppState channel numbers 1-based.
  string(REPLACE
    "                mState.mAudioInChanR = (int) SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_IN_R, CB_GETCURSEL, 0, 0);"
    "                mState.mAudioInChanR = (int) SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_IN_R, CB_GETCURSEL, 0, 0) + 1;"
    _jb_dialog "${_jb_dialog}")
  string(REPLACE
    "                mState.mAudioOutChanR = (int) SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_OUT_R, CB_GETCURSEL, 0, 0);"
    "                mState.mAudioOutChanR = (int) SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_OUT_R, CB_GETCURSEL, 0, 0) + 1;"
    _jb_dialog "${_jb_dialog}")

  # Build the buffer selector from the selected backend. ASIO exposes explicit
  # min/max/preferred/granularity constraints through ASIOGetBufferSize(), so
  # unsupported ASIO sizes are never shown. DirectSound/WASAPI do not advertise
  # a discrete hardware buffer-size list through RtAudio, so they keep iPlug2's
  # normal host-buffer choices and the actual value accepted by openStream is
  # still written back to mState.
  string(REPLACE
    [=[//  Populate buffer size combobox
  for (int i = 0; i< kNumBufferSizeOptions; i++)
  {
    SendDlgItemMessage(hwndDlg,IDC_COMBO_AUDIO_BUF_SIZE,CB_ADDSTRING,0,(LPARAM)kBufferSizeOptions[i].c_str());
  }
  
  WDL_String str;
  str.SetFormatted(32, "%i", mState.mBufferSize);

  LRESULT iovsidx = SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_BUF_SIZE, CB_FINDSTRINGEXACT, -1, (LPARAM) str.Get());
  SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_BUF_SIZE, CB_SETCURSEL, iovsidx, 0);]=]
    [=[// Populate buffer-size combobox. ASIO has real driver constraints;
  // DirectSound/WASAPI use host-side buffer requests rather than a discrete
  // hardware-supported list in RtAudio.
  SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_BUF_SIZE, CB_RESETCONTENT, 0, 0);
  bool jazzbeatBufferComboEnabled = true;
  std::vector<uint32_t> jazzbeatBufferChoices;

#ifdef OS_WIN
  if (mState.mAudioDriverType == kDeviceASIO && mDAC)
  {
    JazzBeatASIOBufferCaps caps;
    std::string rawDriverName;
    const auto selectedID = GetAudioDeviceID(mState.mAudioOutDev.Get());
    if (selectedID)
      rawDriverName = mDAC->getDeviceInfo(selectedID.value()).name;

    const bool alreadyLoaded = mDAC->isStreamOpen();
    if (!rawDriverName.empty() && JazzBeatQueryASIOBufferCaps(rawDriverName.c_str(), alreadyLoaded, caps))
    {
      mState.mBufferSize = JazzBeatNormalizeASIOBuffer(mState.mBufferSize, caps);
      auto addIfAllowed = [&](long value) {
        if (value > 0 && JazzBeatASIOBufferAllowed(value, caps))
          jazzbeatBufferChoices.push_back(static_cast<uint32_t>(value));
      };

      addIfAllowed(caps.minSize);
      addIfAllowed(caps.preferredSize);
      addIfAllowed(mState.mBufferSize);
      for (int i = 0; i < kNumBufferSizeOptions; ++i)
        addIfAllowed(strtol(kBufferSizeOptions[i].c_str(), nullptr, 10));
      addIfAllowed(caps.maxSize);

      std::sort(jazzbeatBufferChoices.begin(), jazzbeatBufferChoices.end());
      jazzbeatBufferChoices.erase(std::unique(jazzbeatBufferChoices.begin(), jazzbeatBufferChoices.end()), jazzbeatBufferChoices.end());
    }
    else
    {
      // If a vendor driver refuses a capability query, do not guess. Keep the
      // currently remembered/accepted value visible but disable manual choices;
      // the Config button remains the authoritative vendor path.
      if (mState.mBufferSize > 0)
        jazzbeatBufferChoices.push_back(mState.mBufferSize);
      jazzbeatBufferComboEnabled = false;
    }
  }
  else
#endif
  {
    for (int i = 0; i < kNumBufferSizeOptions; ++i)
      jazzbeatBufferChoices.push_back(static_cast<uint32_t>(strtoul(kBufferSizeOptions[i].c_str(), nullptr, 10)));
  }

  for (uint32_t frames : jazzbeatBufferChoices)
  {
    WDL_String value;
    value.SetFormatted(32, "%u", frames);
    SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_BUF_SIZE, CB_ADDSTRING, 0, (LPARAM) value.Get());
  }

#ifdef OS_WIN
  ComboBox_Enable(GetDlgItem(hwndDlg, IDC_COMBO_AUDIO_BUF_SIZE), jazzbeatBufferComboEnabled ? TRUE : FALSE);
#endif

  WDL_String str;
  str.SetFormatted(32, "%i", mState.mBufferSize);
  LRESULT iovsidx = SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_BUF_SIZE, CB_FINDSTRINGEXACT, -1, (LPARAM) str.Get());
  if (iovsidx == CB_ERR && !jazzbeatBufferChoices.empty())
  {
    iovsidx = 0;
    mState.mBufferSize = jazzbeatBufferChoices.front();
  }
  SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_BUF_SIZE, CB_SETCURSEL, iovsidx, 0);]=]
    _jb_dialog "${_jb_dialog}")
  string(REPLACE
    [=[            int iovsidx = (int) SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_BUF_SIZE, CB_GETCURSEL, 0, 0);
            mState.mBufferSize = atoi(kBufferSizeOptions[iovsidx].c_str());]=]
    [=[            _this->CloseAudio();
            int iovsidx = (int) SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_BUF_SIZE, CB_GETCURSEL, 0, 0);
            if (iovsidx != CB_ERR)
            {
              char bufferText[32] = {};
              if (SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_BUF_SIZE, CB_GETLBTEXT, iovsidx, (LPARAM) bufferText) != CB_ERR)
              {
                const long requested = strtol(bufferText, nullptr, 10);
                if (requested > 0)
                  mState.mBufferSize = static_cast<uint32_t>(requested);
              }
            }]=]
    _jb_dialog "${_jb_dialog}")

  # Device changes can invalidate a remembered channel pair, sample rate, or
  # ASIO buffer. Re-validate against the newly selected endpoint and repopulate
  # the full audio controls instead of blindly resetting to channels 1/2.
  string(REPLACE
    [=[            // Reset IO
            mState.mAudioInChanL = 1;
            mState.mAudioInChanR = 2;

            _this->PopulateDriverSpecificControls(hwndDlg);]=]
    [=[#ifdef OS_WIN
            // Revalidate after input endpoint change.
            sanitizeAudioStateForCurrentBackend();
#endif
            _this->PopulateAudioDialogs(hwndDlg);]=]
    _jb_dialog "${_jb_dialog}")
  string(REPLACE
    [=[            // Reset IO
            mState.mAudioOutChanL = 1;
            mState.mAudioOutChanR = 2;

            _this->PopulateDriverSpecificControls(hwndDlg);]=]
    [=[#ifdef OS_WIN
            // Revalidate after output endpoint change.
            sanitizeAudioStateForCurrentBackend();
#endif
            _this->PopulateAudioDialogs(hwndDlg);]=]
    _jb_dialog "${_jb_dialog}")

  # If Apply/Config restarted audio while Preferences remains open, changing
  # sample rate stops the live stream before the new value is stored.
  string(REPLACE
    [=[        case IDC_COMBO_AUDIO_SR:
          if (HIWORD(wParam) == CBN_SELCHANGE)
          {
            int idx = (int) SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_SR, CB_GETCURSEL, 0, 0);]=]
    [=[        case IDC_COMBO_AUDIO_SR:
          if (HIWORD(wParam) == CBN_SELCHANGE)
          {
            _this->CloseAudio();
            int idx = (int) SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_SR, CB_GETCURSEL, 0, 0);]=]
    _jb_dialog "${_jb_dialog}")

  # Rebuild the complete RtAudio ASIO backend on Apply/OK when an ASIO setting
  # changes. Reusing the same ASIO driver object across a buffer-size reset is
  # fragile with several vendor drivers. On OK, also commit every remembered
  # per-driver profile to the existing standalone settings.ini.
  string(REPLACE
    [=[        case IDOK:
          if (mActiveState != mState)
            _this->TryToChangeAudio();

          EndDialog(hwndDlg, IDOK); // INI file will be changed see MainDialogProc]=]
    [=[        case IDOK:
#ifdef OS_WIN
          if (mState.mAudioDriverType == kDeviceASIO && mActiveState != mState)
          {
            _this->TryToChangeAudioDriverType();
            _this->ProbeAudioIO();
            sanitizeAudioStateForCurrentBackend();
          }
#endif
          _this->TryToChangeAudio();
#ifdef OS_WIN
          if (mState.mAudioDriverType <= kJazzBeatDeviceWASAPI)
          {
            driverProfiles[mState.mAudioDriverType] = mState;
            driverProfileValid[mState.mAudioDriverType] = true;
          }
          for (int driver = kDeviceDS; driver <= kJazzBeatDeviceWASAPI; ++driver)
            if (driverProfileValid[driver])
              saveDriverProfile(driverProfiles[driver]);
#endif

          EndDialog(hwndDlg, IDOK); // INI file will be changed see MainDialogProc]=]
    _jb_dialog "${_jb_dialog}")
  string(REPLACE
    [=[        case IDAPPLY:
          _this->TryToChangeAudio();
          break;]=]
    [=[        case IDAPPLY:
#ifdef OS_WIN
          if (mState.mAudioDriverType == kDeviceASIO && mActiveState != mState)
          {
            _this->TryToChangeAudioDriverType();
            _this->ProbeAudioIO();
          }
#endif
#ifdef OS_WIN
          sanitizeAudioStateForCurrentBackend();
#endif
          if (_this->TryToChangeAudio())
          {
#ifdef OS_WIN
            if (mState.mAudioDriverType <= kJazzBeatDeviceWASAPI)
            {
              driverProfiles[mState.mAudioDriverType] = mState;
              driverProfileValid[mState.mAudioDriverType] = true;
            }
#endif
            _this->PopulateAudioDialogs(hwndDlg);
          }
          break;]=]
    _jb_dialog "${_jb_dialog}")

  # Since Preferences pauses audio on entry, Cancel must always restore the
  # original backend/state and restart it, even if the user made no changes.
  # This also cleanly undoes settings that may have been auditioned with Apply.
  string(REPLACE
    [=[        case IDCANCEL:
          EndDialog(hwndDlg, IDCANCEL);

          // if state has been changed reset to previous state, INI file won't be changed
          if (!_this->AudioSettingsInStateAreEqual(mState, mTempState)
              || !_this->MIDISettingsInStateAreEqual(mState, mTempState))
          {
            mState = mTempState;

            _this->TryToChangeAudioDriverType();
            _this->ProbeAudioIO();
            _this->TryToChangeAudio();
          }

          break;]=]
    [=[        case IDCANCEL:
          mState = mTempState;
          _this->TryToChangeAudioDriverType();
          _this->ProbeAudioIO();
          _this->TryToChangeAudio();
          EndDialog(hwndDlg, IDCANCEL);
          break;]=]
    _jb_dialog "${_jb_dialog}")

  # Stock iPlug2 calls ASIOControlPanel() while the stream is running (the
  # upstream code labels this path TODO). Preferences now keeps audio stopped,
  # so open the selected vendor driver only for its control panel, read its new
  # preferred buffer, unload it again, and keep audio paused until Apply/OK.
  string(REPLACE
    [=[            if ( (_this->mState.mAudioDriverType == kDeviceASIO) && (_this->mDAC->isStreamRunning() == true)) // TODO: still not right
              ASIOControlPanel();]=]
    [=[            if (_this->mState.mAudioDriverType == kDeviceASIO && _this->mDAC)
            {
              _this->CloseAudio();
              const auto selectedID = _this->GetAudioDeviceID(mState.mAudioOutDev.Get());
              if (selectedID)
              {
                const std::string rawDriverName = _this->mDAC->getDeviceInfo(selectedID.value()).name;
                JazzBeatASIOBufferCaps caps;
                if (JazzBeatOpenASIOControlPanelStopped(rawDriverName.c_str(), caps))
                {
                  mState.mBufferSize = JazzBeatNormalizeASIOBuffer(0, caps);
                  driverProfiles[kDeviceASIO] = mState;
                  driverProfileValid[kDeviceASIO] = true;
                  _this->PopulateAudioDialogs(hwndDlg);
                }
              }
            }]=]
    _jb_dialog "${_jb_dialog}")

  # Switching the Driver combo now restores that backend's in-memory profile
  # instead of resetting every API to device 0 / channels 1-2. The profile is
  # then sanitized against the hardware that actually exists right now.
  string(REPLACE
    [=[            if (v != mState.mAudioDriverType)
            {
              mState.mAudioDriverType = v;

              _this->TryToChangeAudioDriverType();
              _this->ProbeAudioIO();

              if (_this->mAudioInputDevIDs.size())
                mState.mAudioInDev.Set(_this->GetAudioDeviceName(_this->mAudioInputDevIDs[0]).c_str());

              if (_this->mAudioOutputDevIDs.size())
                mState.mAudioOutDev.Set(_this->GetAudioDeviceName(_this->mAudioOutputDevIDs[0]).c_str());

              // Reset IO
              mState.mAudioOutChanL = 1;
              mState.mAudioOutChanR = 2;

              _this->PopulateAudioDialogs(hwndDlg);
            }]=]
    [=[            if (v != mState.mAudioDriverType)
            {
#ifdef OS_WIN
              const int oldDriver = static_cast<int>(mState.mAudioDriverType);
              if (oldDriver >= kDeviceDS && oldDriver <= kJazzBeatDeviceWASAPI)
              {
                driverProfiles[oldDriver] = mState;
                driverProfileValid[oldDriver] = true;
              }

              if (v >= kDeviceDS && v <= kJazzBeatDeviceWASAPI && driverProfileValid[v])
                mState = driverProfiles[v];
              else
              {
                mState = AppState();
                mState.mAudioDriverType = static_cast<uint32_t>(v);
              }
              mState.mAudioDriverType = static_cast<uint32_t>(v);
#else
              mState.mAudioDriverType = v;
#endif

              _this->TryToChangeAudioDriverType();
              _this->ProbeAudioIO();
#ifdef OS_WIN
              sanitizeAudioStateForCurrentBackend();
#endif
              _this->PopulateAudioDialogs(hwndDlg);
            }]=]
    _jb_dialog "${_jb_dialog}")




  # Show the product version in the stock standalone About box.
  # iPlug2 normally displays only copyright + build date.
  string(REPLACE
    [=[info.Append(PLUG_COPYRIGHT_STR"\nBuilt on " __DATE__);]=]
    [=[info.Append(PLUG_COPYRIGHT_STR"\nVersion " PLUG_VERSION_STR "\nBuilt on " __DATE__);]=]
    _jb_dialog "${_jb_dialog}")

  # Fail loudly if a future iPlug2 revision no longer matches the safety patches.
  if(_jb_host STREQUAL _jb_host_original OR _jb_dialog STREQUAL _jb_dialog_original)
    message(FATAL_ERROR "JazzBeat: iPlug2 APP patch did not match this iPlug2 revision")
  endif()
  foreach(_required_text
      "RtAudio::WINDOWS_WASAPI"
      "wait < 50"
      "iParams.firstChannel = (mState.mAudioInChanL > 0)"
      "oParams.firstChannel = (mState.mAudioOutChanL > 0)"
      "RTAUDIO_MINIMIZE_LATENCY"
      "options.numberOfBuffers = 2"
      "mDefaultInputDev.reset()"
      "info.outputChannels >= static_cast<unsigned int>(GetPlug()->MaxNChannels(ERoute::kOutput))"
      "audio_profile_asio"
      "JazzBeatQueryASIOBufferCaps"
      "driverProfiles[mState.mAudioDriverType]"
      "const int oldDriver = static_cast<int>(mState.mAudioDriverType)"
      "jazzbeatBufferChoices"
      "mState.mBufferSize = mBufferSize"
      "JazzBeatASIOBufferAllowed"
      "saveDriverProfile(driverProfiles[driver])"
      "sanitizeAudioStateForCurrentBackend"
      "mTempState = mState;"
      "_this->CloseAudio();\n      _this->PopulatePreferencesDialog"
      "_this->CloseAudio();\n            int iovsidx"
      "case IDCANCEL:"
      "JazzBeatOpenASIOControlPanelStopped"
      "const auto selectedID = _this->GetAudioDeviceID(mState.mAudioOutDev.Get())"
      "Revalidate after input endpoint change"
      "Revalidate after output endpoint change"
      "audio stream open failed"
      "no input buffer"
      "indevidx = outdevidx"
      "audio_profile_wasapi"
      "clampStereoPair"
      "// stereo pair exposed to the APP target"
      "Version "
      "PLUG_VERSION_STR"
      "WASAPI")
    string(FIND "${_jb_host}${_jb_dialog}" "${_required_text}" _found)
    if(_found EQUAL -1)
      message(FATAL_ERROR "JazzBeat: incomplete standalone patch: ${_required_text}")
    endif()
  endforeach()

  set(_jb_patch_dir "${CMAKE_CURRENT_BINARY_DIR}/jazzbeat_iplug_app")
  file(MAKE_DIRECTORY "${_jb_patch_dir}")
  set(_jb_host_patched "${_jb_patch_dir}/IPlugAPP_host.cpp")
  set(_jb_dialog_patched "${_jb_patch_dir}/IPlugAPP_dialog.cpp")
  file(WRITE "${_jb_host_patched}" "${_jb_host}")
  file(WRITE "${_jb_dialog_patched}" "${_jb_dialog}")

  get_target_property(_jb_app_sources iPlug2::APP INTERFACE_SOURCES)
  list(FILTER _jb_app_sources EXCLUDE REGEX "IPlugAPP_host\\.cpp$")
  list(FILTER _jb_app_sources EXCLUDE REGEX "IPlugAPP_dialog\\.cpp$")
  list(APPEND _jb_app_sources "${_jb_host_patched}" "${_jb_dialog_patched}")
  set_property(TARGET iPlug2::APP PROPERTY INTERFACE_SOURCES "${_jb_app_sources}")

  # iPlug2::APP is an INTERFACE IMPORTED target. Its INTERFACE_SOURCES can be
  # patched above, but adding link libraries to it from this subdirectory with
  # target_link_libraries() fails on CMake with "target ... is not built by
  # this project". Apply the WASAPI compile definition and Windows libraries
  # to the concrete standalone executable instead. The transitive APP sources
  # (including RtAudio.cpp) are compiled as sources of this target, so they see
  # __WINDOWS_WASAPI__ as well. VST3 remains completely untouched.
  target_compile_definitions(DerTondehrJazzBeat-app PRIVATE __WINDOWS_WASAPI__)
  target_link_libraries(DerTondehrJazzBeat-app PRIVATE
    ksuser.lib mfplat.lib mfuuid.lib wmcodecdspuuid.lib ole32.lib uuid.lib avrt.lib)
endif()


include(CTest)
if(BUILD_TESTING)
  add_executable(DerTondehrDSPTests tests/dsp_smoke.cpp)
  target_include_directories(DerTondehrDSPTests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${IPLUG2_DIR}/IPlug/Extras"
  )
  if(MSVC)
    target_compile_options(DerTondehrDSPTests PRIVATE /W4 /permissive- /UNDEBUG)
  else()
    target_compile_options(DerTondehrDSPTests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
  endif()
  add_test(NAME dsp_smoke COMMAND DerTondehrDSPTests)

  add_executable(jazzbeat_volume_network_tests tests/volume_network_regression.cpp)
  target_include_directories(jazzbeat_volume_network_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${IPLUG2_DIR}/IPlug/Extras"
  )
  target_compile_features(jazzbeat_volume_network_tests PRIVATE cxx_std_17)
  if(MSVC)
    target_compile_options(jazzbeat_volume_network_tests PRIVATE /W4 /permissive- /UNDEBUG)
  else()
    target_compile_options(jazzbeat_volume_network_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
  endif()
  add_test(NAME volume_network_regression COMMAND jazzbeat_volume_network_tests)

  add_executable(jazzbeat_tone_network_tests tests/tone_network_regression.cpp)
  target_include_directories(jazzbeat_tone_network_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${IPLUG2_DIR}/IPlug/Extras"
  )
  target_compile_features(jazzbeat_tone_network_tests PRIVATE cxx_std_17)
  if(MSVC)
    target_compile_options(jazzbeat_tone_network_tests PRIVATE /W4 /permissive- /UNDEBUG)
  else()
    target_compile_options(jazzbeat_tone_network_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
  endif()
  add_test(NAME tone_network_regression COMMAND jazzbeat_tone_network_tests)

  # Header-only .dtjbpreset format regression test.
  add_executable(jazzbeat_preset_format_tests tests/preset_format_tests.cpp)
  target_include_directories(jazzbeat_preset_format_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
  target_compile_features(jazzbeat_preset_format_tests PRIVATE cxx_std_17)
  if(MSVC)
    target_compile_options(jazzbeat_preset_format_tests PRIVATE /W4 /permissive- /UNDEBUG)
  else()
    target_compile_options(jazzbeat_preset_format_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
  endif()
  add_test(NAME preset_format COMMAND jazzbeat_preset_format_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/presets/Default.dtjbpreset"
    "${CMAKE_CURRENT_SOURCE_DIR}/presets/1991 Black Clean.dtjbpreset")

  add_test(NAME standalone_audio_policy
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/validate_standalone_audio_policy.cmake")
endif()
