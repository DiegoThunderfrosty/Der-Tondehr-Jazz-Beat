# Changelog

## 0.1.28

- Separated the C51/VR5 low-mid shunt node from the VR2 Volume top node.
- Restored approximately 14.2 dB Bass range at 100 Hz and 19.9 dB Treble
  range at 4 kHz under the regression-test settings.
- Added a dedicated Bass/Middle/Treble network regression test.

## 0.1.27

- Corrected the VR2 Volume and C42/VR3 Hi-Treble topology.
- Restored monotonic Volume behavior from near-mute to maximum.
- Removed the obsolete extra digital Hi-Treble shelf.
- Added Volume-network and Hi-Treble interaction regression coverage.

## 0.1.26

- Changed the public topology to one mono input and two stereo outputs.
- Added hardened DirectSound, ASIO and WASAPI Standalone device handling.
- Added per-driver devices, channel, sample-rate and buffer profiles.
- Added bounded audio shutdown, configuration validation and policy tests.

## 0.1.25

- Added the `.dtjbpreset` format with stable IDs and SHA3-256 integrity.
- Added the preset browser, atomic saves and guarded directory traversal.
- Added the included `Default` and `1991 Black Clean` factory presets.

## 0.1.24

- Removed informational subtitle and footer text from the editor.
- Corrected Windows file/product version metadata.
- Added continuous local oversampling in the two nonlinear preamp stages.
- Replaced the lightweight oversampler with polyphase half-band filtering.
- Kept distortion oversampling conditional on the effect being enabled.
- Kept power-stage oversampling adaptive near clipping.
- Added automated x1/x2/x4 behavior and whole-chain performance checks.
- Preserved the monitor-aware fixed editor size and shared VST3/Standalone UI.

Earlier private development history is intentionally omitted from the public
source package because it includes research material and intermediate files not
required to build or understand the released source.
