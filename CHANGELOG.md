# Changelog

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
