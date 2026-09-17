param(
  [string]$BuildDirectory = 'build\windows'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = if ([IO.Path]::IsPathRooted($BuildDirectory)) { $BuildDirectory } else { Join-Path $projectRoot $BuildDirectory }
$bundle = Join-Path $buildRoot 'out\DerTondehrJazzBeat.vst3'
$app = Join-Path $buildRoot 'out\DerTondehrJazzBeat.exe'
$version = '0.1.24'
$removedTexts = @(
  'CIRCUIT-DERIVED STEREO SOLID-STATE AMPLIFIER',
  '3-SPRING REVERB',
  'Speaker/cabinet simulation is intentionally separate',
  'UI CODEPATH'
)

function Get-VST3Binary([string]$bundle) {
  Get-ChildItem -LiteralPath $bundle -Recurse -File |
    Where-Object { $_.Extension -eq '.vst3' } | Sort-Object FullName | Select-Object -First 1
}

$vst3 = Get-VST3Binary $bundle
if (-not $vst3) { throw 'Built VST3 module not found.' }
if (-not (Test-Path -LiteralPath $app)) { throw 'Built Standalone executable not found.' }

foreach ($item in @(@{P=$vst3.FullName; L='VST3'}, @{P=$app; L='Standalone'})) {
  $bytes = [System.IO.File]::ReadAllBytes($item.P)
  $ascii = [System.Text.Encoding]::ASCII.GetString($bytes)
  $unicode = [System.Text.Encoding]::Unicode.GetString($bytes)
  if (-not $ascii.Contains($version) -and -not $unicode.Contains($version)) { throw "$($item.L) does not contain version $version." }
  if (-not $ascii.Contains('Hi-Treble')) { throw "$($item.L) does not contain the continuous Hi-Treble parameter." }
  if (-not $ascii.Contains('LOCAL OVERSAMPLING')) { throw "$($item.L) does not contain the shared oversampling selector." }
  foreach ($mode in @('x1', 'x2', 'x4')) {
    if (-not $ascii.Contains($mode)) { throw "$($item.L) does not contain oversampling mode $mode." }
  }
  foreach ($removedText in $removedTexts) {
    if ($ascii.Contains($removedText)) { throw "$($item.L) still contains removed UI text: $removedText" }
  }
}
Write-Host 'Built VST3 and Standalone contain the shared controls and none of the removed UI texts.' -ForegroundColor Green
