param(
    [switch]$NoOpen
)

$ErrorActionPreference = "Stop"

$env:Path = "C:\Program Files (x86)\Microsoft Visual Studio\Installer;$env:Path"

$vsDevShell = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\Launch-VsDevShell.ps1"
if (-not (Test-Path -LiteralPath $vsDevShell)) { throw "Visual Studio 2022 Build Tools wurden nicht gefunden." }
& $vsDevShell -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null

$env:Path = "C:\Program Files\nodejs;$env:APPDATA\npm;$env:LOCALAPPDATA\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe;$env:Path"

& (Join-Path $PSScriptRoot "build-vst3.ps1") -CleanBuildDir -NoOpen:$NoOpen
