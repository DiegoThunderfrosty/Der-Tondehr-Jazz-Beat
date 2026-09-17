cmake_minimum_required(VERSION 3.24)

project(DerTondehrJazzBeat VERSION 0.1.24 LANGUAGES C CXX)

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
endif()
