#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build-macos"
CONFIGURATION="Release"
INSTALL_AU=false
CLEAN=false

for arg in "$@"; do
    case $arg in
        --clean)   CLEAN=true ;;
        --debug)   CONFIGURATION="Debug" ;;
        --install) INSTALL_AU=true ;;
    esac
done

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$PROJECT_ROOT"

echo "[INFO] Griselda Auto Filter — macOS Build"
echo "[INFO] Konfiguration: $CONFIGURATION"
echo "[INFO] Projektverzeichnis: $PROJECT_ROOT"

command -v cmake  >/dev/null 2>&1 || { echo "[FEHLER] cmake nicht gefunden. Installieren via 'brew install cmake'." >&2; exit 1; }
command -v ninja  >/dev/null 2>&1 || { echo "[FEHLER] ninja nicht gefunden. Installieren via 'brew install ninja'." >&2; exit 1; }

if [ ! -f "$PROJECT_ROOT/JUCE/CMakeLists.txt" ]; then
    echo "[FEHLER] JUCE nicht gefunden unter ./JUCE"
    echo "         Bitte zuerst klonen:"
    echo "         git clone --depth 1 https://github.com/juce-framework/JUCE.git JUCE"
    exit 1
fi

if $CLEAN && [ -d "$BUILD_DIR" ]; then
    echo "[INFO] Build-Verzeichnis wird bereinigt: $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi
mkdir -p "$BUILD_DIR"

echo "[INFO] CMake konfigurieren (Ninja, $CONFIGURATION)..."
cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE="$CONFIGURATION" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"

echo "[INFO] Build starten..."
cmake --build "$BUILD_DIR" --config "$CONFIGURATION"

echo ""
echo "[OK]   Build erfolgreich."

VST3_FILES=$(find "$BUILD_DIR" -name "*.vst3" 2>/dev/null || true)
AU_FILES=$(find "$BUILD_DIR"  -name "*.component" 2>/dev/null || true)

if [ -n "$VST3_FILES" ]; then
    echo "[INFO] VST3-Ausgabe:"
    echo "$VST3_FILES" | while read -r f; do echo "       $f"; done
fi
if [ -n "$AU_FILES" ]; then
    echo "[INFO] AU-Ausgabe:"
    echo "$AU_FILES" | while read -r f; do echo "       $f"; done
fi

if $INSTALL_AU; then
    VST3_SYSTEM="$HOME/Library/Audio/Plug-Ins/VST3"
    AU_SYSTEM="$HOME/Library/Audio/Plug-Ins/Components"
    mkdir -p "$VST3_SYSTEM" "$AU_SYSTEM"

    echo "$VST3_FILES" | while read -r f; do
        [ -n "$f" ] && cp -R "$f" "$VST3_SYSTEM/" && echo "[OK]   VST3 → $VST3_SYSTEM"
    done
    echo "$AU_FILES" | while read -r f; do
        [ -n "$f" ] && cp -R "$f" "$AU_SYSTEM/" && echo "[OK]   AU   → $AU_SYSTEM"
    done
fi
