param(
    [string]$BuildDir = "build-win-x64",
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",
    [string]$Generator = "Ninja",
    [switch]$InstallToSystemVST3,
    [switch]$CleanBuildDir,
    [switch]$Verbose,
    [switch]$NoOpen
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Write-Info([string]$Message)    { Write-Host "[INFO] $Message" -ForegroundColor Cyan }
function Write-WarnMsg([string]$Message) { Write-Host "[WARN] $Message" -ForegroundColor Yellow }
function Write-Ok([string]$Message)      { Write-Host "[OK]   $Message" -ForegroundColor Green }

$projectRoot = Split-Path -Parent $PSCommandPath
if ([string]::IsNullOrWhiteSpace($projectRoot)) { $projectRoot = (Get-Location).Path }
Set-Location -LiteralPath $projectRoot

$cmakeExe = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmakeExe) { throw "cmake wurde nicht gefunden. Bitte CMake installieren." }
if ($Generator -eq "Ninja") {
    $ninjaExe = Get-Command ninja -ErrorAction SilentlyContinue
    if (-not $ninjaExe) { throw "ninja wurde nicht gefunden. Bitte Ninja installieren oder -Generator 'Visual Studio 17 2022' verwenden." }
}

if ($CleanBuildDir -and (Test-Path -LiteralPath $BuildDir)) {
    Write-Info "Build-Verzeichnis wird bereinigt: $BuildDir"
    Remove-Item -LiteralPath $BuildDir -Recurse -Force
}
if (-not (Test-Path -LiteralPath $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

$pnpmExe = Get-Command pnpm -ErrorAction SilentlyContinue
if ($pnpmExe) {
    Write-Info "Oberflaeche bauen (VITE_JUCE_BUILD=1 pnpm build)..."
    Push-Location -LiteralPath (Join-Path $projectRoot "ui")
    try {
        if (-not (Test-Path -LiteralPath "node_modules")) {
            & $pnpmExe.Source install --frozen-lockfile
            if ($LASTEXITCODE -ne 0) { throw "pnpm install fehlgeschlagen." }
        }
        $env:VITE_JUCE_BUILD = "1"
        & $pnpmExe.Source build
        if ($LASTEXITCODE -ne 0) { throw "Build der Oberflaeche fehlgeschlagen." }
    } finally {
        Remove-Item Env:VITE_JUCE_BUILD -ErrorAction SilentlyContinue
        Pop-Location
    }
} else {
    Write-WarnMsg "pnpm wurde nicht gefunden. Es wird das vorhandene ui/dist-juce verwendet."
}

Write-Info "CMake konfigurieren ($Generator, $Configuration)..."
& $cmakeExe.Source -S $projectRoot -B $BuildDir -G $Generator `
    "-DCMAKE_BUILD_TYPE=$Configuration" `
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
if ($LASTEXITCODE -ne 0) { throw "CMake-Konfiguration fehlgeschlagen." }

Write-Info "Build starten..."
$buildArgs = @('--build', $BuildDir, '--config', $Configuration)
if ($Verbose) { $buildArgs += '--verbose' }
& $cmakeExe.Source @buildArgs
if ($LASTEXITCODE -ne 0) { throw "Build fehlgeschlagen." }

Write-Ok "Build erfolgreich abgeschlossen."

$vst3Files = Get-ChildItem -LiteralPath $BuildDir -Filter *.vst3 -Recurse -ErrorAction SilentlyContinue
if ($vst3Files) {
    Write-Info "VST3-Ausgabe:"
    $vst3Files | ForEach-Object { Write-Host "  $($_.FullName)" -ForegroundColor Green }
} else {
    Write-WarnMsg "Keine .vst3-Datei im Build-Ordner gefunden. Pruefen Sie GriseldaAutoFilter_artefacts."
}

if ($vst3Files -and -not $NoOpen) {
    $outDir = Split-Path -Parent (@($vst3Files)[0].FullName)
    Write-Info "Oeffne Ausgabeordner: $outDir"
    Invoke-Item -LiteralPath $outDir
}

if ($InstallToSystemVST3) {
    $systemVst3 = "C:\Program Files\Common Files\VST3"
    foreach ($vst3 in ($vst3Files | Where-Object { $_.PSIsContainer })) {
        try {
            New-Item -ItemType Directory -Path $systemVst3 -Force | Out-Null
            Copy-Item -LiteralPath $vst3.FullName -Destination $systemVst3 -Recurse -Force
            Write-Ok "Installiert nach: $systemVst3"
        } catch {
            Write-WarnMsg "Kopieren nach $systemVst3 fehlgeschlagen (Admin-Rechte erforderlich)."
            Write-WarnMsg "Bitte manuell kopieren: $($vst3.FullName)"
        }
    }
}
