# Der Tondehr Jazz Beat 0.1.26 - mono input / stereo output + safe Standalone drivers

## Audio topology

Jazz Beat now advertises exactly one plug-in input and two outputs (`PLUG_CHANNEL_IO "1-2"`).
The DSP receives one mono sample. Stereo is generated only inside the modeled chorus/twin-power-amp path.
The old `0.5 * (left + right)` host fold-down has been removed.

This applies to both VST3 and Standalone:

- VST3: the host supplies one mono input bus and receives stereo output.
- Standalone: RtAudio opens one physical input channel and two output channels.

## Standalone input channel selection

The existing iPlug2 Preferences input-left combo is repurposed as the canonical mono hardware input selector. It lists every physical input channel (1, 2, 3, ... including the final channel). The legacy right-input combo is disabled and displays `MONO` so the UI does not imply that Jazz Beat consumes a stereo input pair.

`mAudioInChanL` is converted to RtAudio's zero-based `firstChannel`. Therefore selecting input 4 opens physical channel 4 with `firstChannel = 3`. Output remains a contiguous stereo pair driven by `mAudioOutChanL/mAudioOutChanR`.

## Windows driver system

The Standalone target carries the hardened driver architecture already used by Der Tondehr Crunchy, adapted to Jazz Beat's 1-in/2-out topology:

- DirectSound, ASIO and WASAPI backends.
- DirectSound low-latency mode with a two-buffer queue.
- Per-driver profiles for devices, mono input channel, stereo output pair, sample rate and buffer size.
- Current hardware is sanitized whenever the API/device changes.
- Input/output must share a supported sample rate; fallbacks prefer device rates, 48 kHz, 44.1 kHz, then the first common rate.
- ASIO buffer choices come from `ASIOGetBufferSize()` min/max/preferred/granularity instead of invented sizes.
- ASIO vendor Config opens only while the stream is stopped and the preferred buffer is read back afterward.
- RtAudio's actually accepted buffer size is persisted after `openStream()`.
- Preferences pauses audio on entry. Apply can audition a setup, OK commits it, and Cancel reconstructs and restarts the exact state that was active before Preferences opened.
- Audio shutdown is bounded; a dead/unplugged backend cannot block forever waiting for another callback.
- Release builds show stream-open/start/backend errors in a MessageBox.
- A missing capture buffer is explicitly detected and outputs silence rather than passing a null input into the DSP.

## VST3 isolation

WASAPI/RtAudio Windows backend compile definitions and system libraries are attached only to `DerTondehrJazzBeat-app`. They are not attached to the VST3 target; the DAW remains responsible for hardware audio I/O.

## Validation

`scripts/validate_standalone_audio_policy.cmake` checks the 1->2 plug-in layout, mono SignalChain API, absence of the old stereo fold-down, APP-only driver patches, ASIO safety helpers, per-driver profiles, DirectSound low-latency settings, selected physical-channel routing, and that WASAPI libraries/definitions are not attached directly to VST3.
