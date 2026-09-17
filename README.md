# Der Tondehr Jazz Beat

Der Tondehr Jazz Beat is an independent, open-source stereo guitar-amplifier
plugin inspired by aspects of the circuit architecture and control layout of
the Roland Jazz Chorus amplifier family. It is an unofficial technical and
creative project. It is not affiliated with, sponsored by, endorsed by, or
approved by Roland Corporation. The referenced product and company names are
used only to identify the design inspiration; they are not part of this
project's branding.

The project is written in C++17 with iPlug2 and currently builds two Windows
x64 products:

- `DerTondehrJazzBeat.vst3`, for compatible VST3 hosts.
- `DerTondehrJazzBeat.exe`, a standalone application.

The project models the electrical amplifier section. It does not include a
speaker cabinet or impulse response. A typical recording chain is:

```text
Guitar or DI -> Der Tondehr Jazz Beat -> cabinet/IR loader -> output
```

## Main features

- High/Low input behavior and ±24 dB input trim.
- Switchable diode-style distortion.
- Interactive Bass, Middle and Treble network.
- Continuous Hi-Treble control.
- Spring-reverb model.
- Fixed and manual BBD-style stereo chorus modes.
- Dual modeled power-amplifier paths.
- Local x1, x2 and x4 oversampling around nonlinear stages only.
- −24 dB to +12 dB output trim.
- One fixed proportional editor size selected from the active monitor and DPI.
- Shared DSP and editor source for VST3 and Standalone.

## Current status

This source package contains version `0.1.24` and targets Windows x64. The
included automated tests exercise the signal chain, control ranges, editor
scaling, output trim and oversampling behavior.

## Repository contents

```text
Der-Tondehr-Jazz-Beat/
|-- .github/               Issue, pull-request and CI files
|-- docs/licenses/         Copies of primary dependency licenses
|-- resources/             Windows resources
|-- scripts/               Dependency, build and verification scripts
|-- src/                   Plugin, DSP and editor source
|-- tests/                 DSP and scaling tests
|-- .gitattributes         Text normalization
|-- .gitignore             Generated/private-file exclusions
|-- BUILDING.md            Short build guide
|-- CHANGELOG.md           Public release history
|-- CONTRIBUTING.md        Contribution requirements
|-- LICENSE                MIT License for original project code
|-- SECURITY.md            Security-reporting policy
|-- THIRD_PARTY.md         Dependency attribution
|-- CMakeLists.txt         Main build configuration
`-- README.md              This document
```

Downloaded dependencies, local builds, circuit drawings, manuals, audio,
sessions and private development notes are intentionally excluded.

## Requirements

1. Windows 10 or Windows 11, 64-bit.
2. Git for Windows.
3. Visual Studio with **Desktop development with C++**.
4. MSVC x64 tools, a recent Windows SDK and CMake tools for Windows.
5. PowerShell 5.1 or later.
6. Internet access for the first dependency setup.

## Build on a clean PC

Clone the repository and enter it:

```powershell
git clone https://github.com/DiegoThunderfrosty/Der-Tondehr-Jazz-Beat.git
cd Der-Tondehr-Jazz-Beat
```

Download the pinned iPlug2 and VST3 SDK revisions:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\setup_dependencies.ps1
```

Build VST3, Standalone and tests in Release mode:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

Expected output:

```text
build\windows\out\DerTondehrJazzBeat.vst3\
build\windows\out\DerTondehrJazzBeat.exe
```

The first item is a complete VST3 bundle. Copy the complete bundle to:

```text
C:\Program Files\Common Files\VST3
```

Close the audio host before replacing an existing build, then reopen it and
perform a complete plugin rescan. Keep only one copy in host-scanned folders.

## Oversampling scope

Oversampling is deliberately local. It surrounds the nonlinear preamplifier
devices, enabled distortion and the power stage near clipping. Tone filtering,
volume, spring tank, BBD delay and other linear processing remain at the host
sample rate. x2 uses one polyphase half-band stage; x4 uses two cascaded stages.

## Updating an existing checkout

```powershell
git pull
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\setup_dependencies.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

## Troubleshooting

### Git, CMake or the compiler is not found

Install Git for Windows and the Visual Studio **Desktop development with C++**
workload. Close and reopen PowerShell after changing installed components.

### iPlug2 or VST3 SDK is missing

Run `scripts\setup_dependencies.ps1`. Do not create empty directories with the
expected names; the script checks the pinned repositories and required files.

### The plugin is not listed

Confirm that the complete `.vst3` bundle is in the system VST3 directory,
remove older duplicates and run the host's full rescan.

### Audio breaks up

Increase the host/standalone buffer size. x4 oversampling intentionally uses
more CPU than x1. CPU percentages depend on sample rate, buffer size and host.

## Contributions

Read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting changes. Do not add
third-party manuals, circuit drawings, logos, recordings, impulse responses or
other material that you are not permitted to redistribute.

## License

Original project code is released under the [MIT License](LICENSE), Copyright
(c) 2026 Diego Rodriguez. Dependencies retain their own licenses and notices;
see [THIRD_PARTY.md](THIRD_PARTY.md).
