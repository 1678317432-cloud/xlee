$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$BuildDir = Join-Path $Root "build\SuperBass-Windows"
$InstallDir = Join-Path $env:USERPROFILE "Downloads\SuperBass-Windows-VST3"

cmake -S (Join-Path $Root "source\SuperBass") `
    -B $BuildDir `
    -G "Visual Studio 17 2022" `
    -A x64 `
    -DCMAKE_BUILD_TYPE=Release

cmake --build $BuildDir --config Release --parallel

$Vst3 = Get-ChildItem -Path $BuildDir -Recurse -Filter "Super Bass.vst3" | Select-Object -First 1
if (-not $Vst3) {
    throw "Super Bass.vst3 was not produced. Check the CMake/JUCE VST3 target output."
}

New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
Copy-Item -Recurse -Force $Vst3.FullName $InstallDir

Write-Host "Windows VST3 exported to: $InstallDir"
