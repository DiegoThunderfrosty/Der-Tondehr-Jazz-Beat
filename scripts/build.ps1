param(
  [string]$IPlug2Dir,
  [string]$Configuration = 'Release',
  [switch]$SkipTests,
  [string]$BuildDirectory = 'build\windows'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $IPlug2Dir) { $IPlug2Dir = Join-Path $projectRoot 'external\iPlug2' }
if (-not (Test-Path -LiteralPath (Join-Path $IPlug2Dir 'iPlug2.cmake'))) {
  throw 'iPlug2 was not found. Run scripts\setup_dependencies.ps1 first.'
}
foreach ($sdkFile in @(
  'Dependencies\IPlug\VST3_SDK\base\source\fobject.cpp',
  'Dependencies\IPlug\VST3_SDK\pluginterfaces\base\funknown.h',
  'Dependencies\IPlug\VST3_SDK\public.sdk\source\vst\vstsinglecomponenteffect.cpp'
)) {
  if (-not (Test-Path -LiteralPath (Join-Path $IPlug2Dir $sdkFile))) {
    throw 'The required VST3 SDK components were not found inside iPlug2. Run scripts\setup_dependencies.ps1.'
  }
}
$IPlug2Dir = (Resolve-Path -LiteralPath $IPlug2Dir).Path

$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmakeCommand) {
  $cmakePath = $cmakeCommand.Source
} else {
  $cmakePath = Get-ChildItem -LiteralPath 'C:\Program Files\Microsoft Visual Studio' -Recurse -Filter cmake.exe -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match '\\CMake\\bin\\cmake.exe$' } |
    Select-Object -First 1 -ExpandProperty FullName
}
if (-not $cmakePath) { throw 'CMake was not found. Install Visual Studio C++ and CMake tools.' }

$buildDir = if ([IO.Path]::IsPathRooted($BuildDirectory)) { $BuildDirectory } else { Join-Path $projectRoot $BuildDirectory }
$testFlag = if ($SkipTests) { 'OFF' } else { 'ON' }

& $cmakePath -S $projectRoot -B $buildDir -A x64 `
  "-DIPLUG2_DIR=$IPlug2Dir" "-DBUILD_TESTING=$testFlag" '-DIPLUG_DEPLOY_PLUGINS=OFF'
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }

$targets = @('DerTondehrJazzBeat-vst3', 'DerTondehrJazzBeat-app')
if (-not $SkipTests) { $targets += 'DerTondehrDSPTests' }
& $cmakePath --build $buildDir --config $Configuration --target $targets --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }

if (-not $SkipTests) {
  $ctestPath = Join-Path (Split-Path -Parent $cmakePath) 'ctest.exe'
  if (-not (Test-Path -LiteralPath $ctestPath)) {
    $ctestPath = (Get-Command ctest -ErrorAction Stop).Source
  }
  & $ctestPath --test-dir $buildDir -C $Configuration --output-on-failure
  if ($LASTEXITCODE -ne 0) { throw 'One or more tests failed.' }
}

Write-Host "Build is ready: $buildDir\out"
