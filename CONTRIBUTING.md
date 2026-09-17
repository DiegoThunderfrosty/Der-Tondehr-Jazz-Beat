# Contributing

Thank you for considering a contribution to Der Tondehr Jazz Beat.

## Before starting

1. Search for an existing issue.
2. Propose changes affecting sound, parameters, identifiers or saved sessions
   before implementing them.
3. Create a branch from `main` and keep each change focused.

## Build and validation

Follow [BUILDING.md](BUILDING.md), build Release and run:

```powershell
ctest --test-dir build\windows -C Release --output-on-failure
```

For audio changes, report sample rate, buffer size, oversampling mode and all
relevant control settings.

## Code rules

- Keep the project compatible with C++17.
- Do not allocate, lock, access files or open UI dialogs on the audio thread.
- Do not change existing parameter IDs without an explicit migration.
- Keep output finite for invalid input or host data.
- Keep oversampling local to genuinely nonlinear operations.
- Document perceptual constants that are not direct circuit values.
- Preserve shared processing behavior between VST3 and Standalone.

## External material

Do not add third-party manuals, circuit drawings, logos, recordings, impulse
responses, presets or other files without redistribution permission. Do not use
the referenced manufacturer or amplifier name in repository branding, package
names, filenames or logos.

## Pull requests

Describe the problem, previous/new behavior, main files, tests and any sound,
compatibility, CPU, latency or migration risk. Contributions are distributed
under the project's MIT License.
