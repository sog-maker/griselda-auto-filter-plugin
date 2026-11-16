# Griselda Auto Filter bauen

[English](BUILD.md)

## Die Teile

| Pfad | Was es ist |
| --- | --- |
| `CMakeLists.txt` | der Build: JUCE-Plug-in, VST3 und Standalone, die Oberfläche als Binärdaten |
| `PluginProcessor.h/.cpp` | die DSP-Engine, die Parameter, der gespeicherte Zustand |
| `PluginEditor.h/.cpp` | der WebView2-Editor, die Brücke zur Oberfläche, Presets und XML-Export/-Import |
| `ui/` | die Oberfläche in React, mit Vite nach `ui/dist-juce` gebaut |
| `rebuild.ps1` | ein sauberer Komplett-Build unter Windows |
| `build-vst3.ps1` | das Build-Skript für Windows |
| `build-vst3.sh` | das Build-Skript für macOS |

Zuerst wird die Oberfläche gebaut. CMake packt `ui/dist-juce` dann in das Plug-in, es braucht also keine Dateien
daneben.

## Windows

### Werkzeuge

| Werkzeug | Hinweis |
| --- | --- |
| Visual Studio 2022 oder die Build Tools | Workload *Desktopentwicklung mit C++* |
| [CMake](https://cmake.org) ab 3.22 | |
| [Ninja](https://ninja-build.org) | |
| [Node.js](https://nodejs.org) und pnpm 10 | `npm install -g pnpm@10` |
| JUCE 8.0.13 | nach `./JUCE` geklont |
| WebView2 SDK 1.0.3485.44 | NuGet-Paket, siehe unten |

### Einrichten

Im Hauptordner des Repositorys:

```powershell
git clone --depth 1 --branch 8.0.13 https://github.com/juce-framework/JUCE.git JUCE
```

Das WebView2 SDK installieren; JUCE findet es in den NuGet-Paketen des Benutzers:

```powershell
Register-PackageSource -Provider NuGet -Name nugetRepository -Location https://www.nuget.org/api/v2
Install-Package Microsoft.Web.WebView2 -Scope CurrentUser -RequiredVersion 1.0.3485.44 -Source nugetRepository
```

### Bauen

```powershell
powershell -ExecutionPolicy Bypass -File .\rebuild.ps1
```

`rebuild.ps1` lädt die Visual-Studio-Umgebung, löscht `build-win-x64`, baut die Oberfläche
(`VITE_JUCE_BUILD=1 pnpm build`) und danach das Plug-in. Die Ergebnisse:

```
build-win-x64\GriseldaAutoFilter_artefacts\Release\VST3\Griselda Auto Filter.vst3
build-win-x64\GriseldaAutoFilter_artefacts\Release\Standalone\Griselda Auto Filter.exe
```

`rebuild.ps1` erwartet die Visual Studio 2022 Build Tools im Standardordner. Liegt Visual Studio woanders, die
*x64 Native Tools Command Prompt for VS 2022* öffnen, dort PowerShell starten und `.\build-vst3.ps1` aufrufen.

`build-vst3.ps1` kennt diese Optionen:

| Option | Bedeutung |
| --- | --- |
| `-Configuration Debug` | ein Debug-Build (Standard `Release`) |
| `-BuildDir NAME` | ein anderer Build-Ordner (Standard `build-win-x64`) |
| `-Generator "Visual Studio 17 2022"` | ohne Ninja bauen |
| `-CleanBuildDir` | den Build-Ordner vorher löschen |
| `-InstallToSystemVST3` | die VST3 nach `C:\Program Files\Common Files\VST3` kopieren (braucht Administratorrechte) |
| `-NoOpen` | den Ausgabeordner danach nicht öffnen |

## macOS (ungetestet)

Der Code lässt sich mit JUCE auf macOS bauen, die aktuelle Version ist dort aber nicht getestet. Nötig sind die
Xcode-Kommandozeilenwerkzeuge, [Homebrew](https://brew.sh), CMake, Ninja, Node.js und pnpm.

```bash
xcode-select --install
brew install cmake ninja node
npm install -g pnpm@10
git clone --depth 1 --branch 8.0.13 https://github.com/juce-framework/JUCE.git JUCE
cd ui && pnpm install --frozen-lockfile && VITE_JUCE_BUILD=1 pnpm build && cd ..
./build-vst3.sh --clean
```

`build-vst3.sh` baut ein Universal Binary (arm64 und x86_64) nach `build-macos`. `--install` kopiert die VST3 nach
`~/Library/Audio/Plug-Ins/VST3`. Blockiert Gatekeeper einen unsignierten Build:

```bash
xattr -dr com.apple.quarantine "$HOME/Library/Audio/Plug-Ins/VST3/Griselda Auto Filter.vst3"
```

## Nur die Oberfläche

```powershell
cd ui
pnpm install --frozen-lockfile
pnpm build
```

Ohne `VITE_JUCE_BUILD=1` schreibt Vite mit absoluten Pfaden nach `ui/dist`, für einen schnellen Blick im Browser. Das
Plug-in braucht den Build mit `VITE_JUCE_BUILD=1`, der relative Pfade nach `ui/dist-juce` schreibt.

## Die Brücke zwischen Oberfläche und Plug-in

Die Oberfläche schickt Regleränderungen als JSON an JUCE:

```json
{ "type": "SET_PARAM", "id": "cutoff", "value": 0.75 }
```

Das Plug-in schickt Automation vom Host als Ereignis zurück:

```js
window.dispatchEvent(new CustomEvent("paramUpdate", { detail: { id: "cutoff", value: 0.75 } }));
```

Die Werte sind normalisiert (0 bis 1). Anzeigetexte kommen mit dem Ereignis `paramText`, das Spektrum mit `spectrum`.
Ist die Seite geladen, schickt sie `{ "type": "READY" }`, und das Plug-in antwortet mit dem aktuellen Zustand.

## Einen Build prüfen

[pluginval](https://github.com/Tracktion/pluginval) prüft die VST3:

```powershell
pluginval --strictness-level 5 --validate "build-win-x64\GriseldaAutoFilter_artefacts\Release\VST3\Griselda Auto Filter.vst3"
```
