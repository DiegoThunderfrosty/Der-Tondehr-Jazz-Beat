#pragma once

#define PLUG_NAME "Der Tondehr Jazz Beat"
#define PLUG_MFR "Der Tondehr"
#define PLUG_VERSION_HEX 0x00011C00
#define PLUG_VERSION_STR "0.1.28"
#define PLUG_UNIQUE_ID 'DTJB'
#define PLUG_MFR_ID 'DTon'
#define PLUG_URL_STR "https://example.invalid/der-tondehr"
#define PLUG_EMAIL_STR ""
#define PLUG_COPYRIGHT_STR "Copyright 2026 Der Tondehr"
#define PLUG_CLASS_NAME DerTondehrJazzBeat

#define BUNDLE_NAME "DerTondehrJazzBeat"
#define BUNDLE_MFR "DerTondehr"
#define BUNDLE_DOMAIN "com"
#define SHARED_RESOURCES_SUBPATH "DerTondehrJazzBeat"

#if defined(APP_API)
  // Standalone captures the stereo hardware pair selected in Preferences.
  // MONO IN 1/2 chooses which member feeds Jazz Beat's mono front-end.
  #define PLUG_CHANNEL_IO "2-2"
#else
  // The plug-in presented to a DAW is natively mono-in / stereo-out.
  #define PLUG_CHANNEL_IO "1-2"
#endif
#define PLUG_LATENCY 0
#define PLUG_TYPE 0
#define PLUG_DOES_MIDI_IN 0
#define PLUG_DOES_MIDI_OUT 0
#define PLUG_DOES_MPE 0
#define PLUG_DOES_STATE_CHUNKS 0
#define PLUG_HAS_UI 1
#define PLUG_WIDTH 1140
#define PLUG_HEIGHT 510
#define PLUG_FPS 60
#define PLUG_SHARED_RESOURCES 0
// The host cannot resize the editor. Its fixed size is selected automatically
// from the active monitor's work area and DPI when the plug-in is instantiated.
#define PLUG_HOST_RESIZE 0

#define AUV2_ENTRY DerTondehrJazzBeat_Entry
#define AUV2_ENTRY_STR "DerTondehrJazzBeat_Entry"
#define AUV2_FACTORY DerTondehrJazzBeat_Factory
#define AUV2_VIEW_CLASS DerTondehrJazzBeat_View
#define AUV2_VIEW_CLASS_STR "DerTondehrJazzBeat_View"

#define VST3_SUBCATEGORY "Fx|Distortion|Reverb|Modulation"

#define APP_NUM_CHANNELS 2
#define APP_N_VECTOR_WAIT 0
#define APP_MULT 1
#define APP_COPY_AUV3 0
#define APP_SIGNAL_VECTOR_SIZE 64
