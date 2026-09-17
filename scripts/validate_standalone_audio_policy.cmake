set(ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
file(READ "${ROOT}/CMakeLists.txt" AUDIO_CMAKE)
file(READ "${ROOT}/config.h" CONFIG_H)
file(READ "${ROOT}/src/DerTondehrJazzBeat.cpp" AUDIO_CPP)
file(READ "${ROOT}/src/dsp/SignalChain.h" SIGNAL_H)

set(REQUIRED_CMAKE
  "if(WIN32 AND TARGET iPlug2::APP)"
  "RtAudio::WINDOWS_WASAPI"
  "target_compile_definitions(DerTondehrJazzBeat-app PRIVATE __WINDOWS_WASAPI__)"
  "RTAUDIO_MINIMIZE_LATENCY"
  "options.numberOfBuffers = 2"
  "iParams.nChannels = GetPlug()->MaxNChannels(ERoute::kInput); // stereo pair exposed to the APP target"
  "iParams.firstChannel = (mState.mAudioInChanL > 0)"
  "oParams.firstChannel = (mState.mAudioOutChanL > 0)"
  "audio_profile_directsound"
  "audio_profile_asio"
  "audio_profile_wasapi"
  "JazzBeatQueryASIOBufferCaps"
  "JazzBeatASIOBufferAllowed"
  "JazzBeatOpenASIOControlPanelStopped"
  "sanitizeAudioStateForCurrentBackend"
  "clampStereoPair"
  "info.outputChannels >= static_cast<unsigned int>(GetPlug()->MaxNChannels(ERoute::kOutput))"
  "_this->CloseAudio();"
  "audio stream open failed"
  "audio stream start failed"
  "no input buffer"
  "target_link_libraries(DerTondehrJazzBeat-app PRIVATE"
  "ksuser.lib"
  "avrt.lib"
)
foreach(token IN LISTS REQUIRED_CMAKE)
  string(FIND "${AUDIO_CMAKE}" "${token}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "Standalone audio integration missing token: ${token}")
  endif()
endforeach()

string(FIND "${CONFIG_H}" "#if defined(APP_API)" app_cond)
string(FIND "${CONFIG_H}" "#define PLUG_CHANNEL_IO \"2-2\"" app_layout)
string(FIND "${CONFIG_H}" "#define PLUG_CHANNEL_IO \"1-2\"" vst_layout)
if(app_cond EQUAL -1 OR app_layout EQUAL -1 OR vst_layout EQUAL -1)
  message(FATAL_ERROR "Jazz Beat must expose APP 2-2 capture plus VST3 1-2 layout")
endif()

set(REQUIRED_MONO_DSP
  "StereoSample process(double monoInput)"
  "auto out = mSignalChain.process(monoIn);"
  "NInChansConnected()"
  "StandaloneMonoInputControl"
  "MONO IN"
  "mStandaloneMonoInputBlend"
  "standalone_audio.txt"
)
foreach(token IN LISTS REQUIRED_MONO_DSP)
  string(FIND "${AUDIO_CPP}${SIGNAL_H}" "${token}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "Mono-input DSP integration missing token: ${token}")
  endif()
endforeach()

string(FIND "${SIGNAL_H}" "0.5 * (inputLeft + inputRight)" stereo_fold)
if(NOT stereo_fold EQUAL -1)
  message(FATAL_ERROR "Stereo fold-down must not remain in Jazz Beat SignalChain")
endif()


string(FIND "${AUDIO_CMAKE}" "EnableWindow(GetDlgItem(hwndDlg, IDC_COMBO_AUDIO_IN_R), FALSE)" disabled_input_r)
if(NOT disabled_input_r EQUAL -1)
  message(FATAL_ERROR "Input 2 (R) must remain enabled; MONO IN 1/2 selects which captured input feeds the mono DSP")
endif()

string(FIND "${AUDIO_CMAKE}" "target_link_libraries(DerTondehrJazzBeat-vst3" vst3_link)
if(NOT vst3_link EQUAL -1)
  message(FATAL_ERROR "Standalone backend libraries must not be linked directly to VST3")
endif()
string(FIND "${AUDIO_CMAKE}" "target_compile_definitions(DerTondehrJazzBeat-vst3 PRIVATE __WINDOWS_WASAPI__)" vst3_wasapi)
if(NOT vst3_wasapi EQUAL -1)
  message(FATAL_ERROR "WASAPI backend compile definition must remain Standalone-only")
endif()

message(STATUS "Jazz Beat mono-input/stereo-output standalone audio policy validation passed")
