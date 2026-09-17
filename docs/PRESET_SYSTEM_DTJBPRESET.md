# Der Tondehr Jazz Beat .dtjbpreset system (current in 0.1.28)

Jazz Beat now has a product-specific preset container that is independent from iPlug2/VST3 session state.

## File identity

- Extension: `.dtjbpreset`
- Magic: `DTJBPRS1`
- Product marker: `JAZZBEAT`
- Container version: 1
- Payload version: 1
- Integrity algorithm: SHA3-256 (`0x00030001`)
- Digest domain: `DerTondehrJazzBeatPreset-v1`
- Maximum file size: 64 KiB

Renaming another file to `.dtjbpreset` is not enough to load it. Magic, product, versions, declared integrity algorithm, payload size, SHA3-256 digest, stable control IDs, control types and value ranges are all validated before any parameter is changed.

## Stored controls

The format stores exactly 15 user-facing sound controls. Each uses a stable preset ID independent of the iPlug2/VST3 ParamID ordering.

`kHiTrebleLegacy` (VST3 ParamID 7) is deliberately never serialized. It remains migration-only state for old DAW sessions. The current continuous `kHiTreble` control is serialized normally.

Window/UI scale, audio driver/device settings, preset directory, DSP histories, reverb/chorus buffers and transport state are not part of a preset.

## Recall behavior

The complete file is validated before recall. The audio thread is then briefly gated while all 15 parameters are committed as one transaction. `SignalChain` receives the new parameter set, its history is reset once, and Jazz Beat's existing short de-click/fade-in mechanism is armed. This prevents an audio block from seeing a half-applied preset.

The VST3/DAW state mechanism remains unchanged; `.dtjbpreset` is an additional manual preset layer, not a replacement for host project recall.

## Browser and persistence

The UI bar provides preset field, `+`, `-`, `SAVE`, and `DELETE`. The selected root directory is persisted in:

`Der Tondehr/DerTondehrJazzBeat/preset_directory.txt`

Only `.dtjbpreset` regular files are listed. The browser supports subfolders, `..`, single-click selection, double-click recall, polling for external folder changes, a 4096-entry folder cap, overwrite confirmation and delete confirmation.

The path layer blocks symbolic links / Windows reparse points and refuses to leave the configured root. Saves use a temporary file plus an atomic replacement; Windows uses write-through replacement/move APIs.
