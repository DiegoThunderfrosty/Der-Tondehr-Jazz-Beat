# Building

The complete clean-PC procedure is in [README.md](README.md#build-on-a-clean-pc).

On a Windows computer that already has Git, Visual Studio C++ tools, a Windows
SDK, CMake and PowerShell:

```powershell
git clone https://github.com/DiegoThunderfrosty/Der-Tondehr-Jazz-Beat.git
cd Der-Tondehr-Jazz-Beat
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\setup_dependencies.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

Expected output:

```text
build\windows\out\DerTondehrJazzBeat.vst3\
build\windows\out\DerTondehrJazzBeat.exe
```

Dependency revisions are pinned in `scripts/setup_dependencies.ps1`. Update
them only after building both formats and running the complete test suite.
