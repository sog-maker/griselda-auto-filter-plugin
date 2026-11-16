# Building Griselda Auto Filter

[Deutsch](BUILD.de.md)

## The parts

| Path | What it is |
| --- | --- |
| `CMakeLists.txt` | the build: JUCE plug-in target, VST3 and standalone, the interface as binary data |
| `PluginProcessor.h/.cpp` | the DSP engine, the parameters, the saved state |
| `PluginEditor.h/.cpp` | the WebView2 editor, the bridge to the interface, presets and XML export/import |
| `ui/` | the interface in React, built with Vite into `ui/dist-juce` |
| `rebuild.ps1` | a clean full build on Windows |
| `build-vst3.ps1` | the Windows build script |
| `build-vst3.sh` | the macOS build script |

The interface is built first. CMake then packs `ui/dist-juce` into the plug-in, so the plug-in needs no files next to it.

## Windows

### Tools

| Tool | Note |
| --- | --- |
| Visual Studio 2022 or the Build Tools | workload *Desktop development with C++* |
| [CMake](https://cmake.org) 3.22 or newer | |
| [Ninja](https://ninja-build.org) | |
| [Node.js](https://nodejs.org) and pnpm 10 | `npm install -g pnpm@10` |
| JUCE 8.0.13 | cloned into `./JUCE` |
| WebView2 SDK 1.0.3485.44 | NuGet package, see below |

### Set up

From the root of the repository:

```powershell
git clone --depth 1 --branch 8.0.13 https://github.com/juce-framework/JUCE.git JUCE
```

Install the WebView2 SDK, which JUCE finds in the user's NuGet packages:

```powershell
Register-PackageSource -Provider NuGet -Name nugetRepository -Location https://www.nuget.org/api/v2
Install-Package Microsoft.Web.WebView2 -Scope CurrentUser -RequiredVersion 1.0.3485.44 -Source nugetRepository
```

### Build

```powershell
powershell -ExecutionPolicy Bypass -File .\rebuild.ps1
```

`rebuild.ps1` loads the Visual Studio build environment, deletes `build-win-x64`, builds the interface
(`VITE_JUCE_BUILD=1 pnpm build`) and then the plug-in. The results:

```
build-win-x64\GriseldaAutoFilter_artefacts\Release\VST3\Griselda Auto Filter.vst3
build-win-x64\GriseldaAutoFilter_artefacts\Release\Standalone\Griselda Auto Filter.exe
```

`rebuild.ps1` expects the Visual Studio 2022 Build Tools in their default folder. If Visual Studio is somewhere else,
open the *x64 Native Tools Command Prompt for VS 2022*, start PowerShell there and run `.\build-vst3.ps1`.

`build-vst3.ps1` takes these options:

| Option | Meaning |
| --- | --- |
| `-Configuration Debug` | a debug build (default `Release`) |
| `-BuildDir NAME` | another build folder (default `build-win-x64`) |
| `-Generator "Visual Studio 17 2022"` | build without Ninja |
| `-CleanBuildDir` | delete the build folder first |
| `-InstallToSystemVST3` | copy the VST3 to `C:\Program Files\Common Files\VST3` (needs administrator rights) |
| `-NoOpen` | do not open the output folder afterwards |

## macOS (untested)

The code builds with JUCE on macOS, but the current version has not been tested there. You need the Xcode command line
tools, [Homebrew](https://brew.sh), CMake, Ninja, Node.js and pnpm.

```bash
xcode-select --install
brew install cmake ninja node
npm install -g pnpm@10
git clone --depth 1 --branch 8.0.13 https://github.com/juce-framework/JUCE.git JUCE
cd ui && pnpm install --frozen-lockfile && VITE_JUCE_BUILD=1 pnpm build && cd ..
./build-vst3.sh --clean
```

`build-vst3.sh` builds a universal binary (arm64 and x86_64) into `build-macos`. `--install` copies the VST3 to
`~/Library/Audio/Plug-Ins/VST3`. If Gatekeeper blocks an unsigned build:

```bash
xattr -dr com.apple.quarantine "$HOME/Library/Audio/Plug-Ins/VST3/Griselda Auto Filter.vst3"
```

## The interface on its own

```powershell
cd ui
pnpm install --frozen-lockfile
pnpm build
```

Without `VITE_JUCE_BUILD=1` Vite writes to `ui/dist` with absolute paths, for a quick look in a browser. The plug-in
needs the build with `VITE_JUCE_BUILD=1`, which writes relative paths to `ui/dist-juce`.

## The bridge between interface and plug-in

The interface sends knob changes to JUCE as JSON:

```json
{ "type": "SET_PARAM", "id": "cutoff", "value": 0.75 }
```

The plug-in sends automation from the host back as an event:

```js
window.dispatchEvent(new CustomEvent("paramUpdate", { detail: { id: "cutoff", value: 0.75 } }));
```

Values are normalised from 0 to 1. The display texts come with the event `paramText`, the spectrum with `spectrum`.
When the page has loaded it sends `{ "type": "READY" }`, and the plug-in answers with the current state.

## Checking a build

[pluginval](https://github.com/Tracktion/pluginval) checks the VST3:

```powershell
pluginval --strictness-level 5 --validate "build-win-x64\GriseldaAutoFilter_artefacts\Release\VST3\Griselda Auto Filter.vst3"
```
