# Installing Griselda Auto Filter

[Deutsch](INSTALL.de.md)

## What you need

- Windows 10 or 11, 64-bit
- a DAW that loads VST3 plug-ins (tested in Ableton Live 12), or none for the standalone application
- the Microsoft Edge WebView2 Runtime (Windows 11 has it)

## Step by step

1. Open the [latest release](https://github.com/sog-maker/griselda-auto-filter-plugin/releases/latest) and download
   the Windows zip.
2. Unpack the zip. It holds the folder `Griselda Auto Filter.vst3` and the program `Griselda Auto Filter.exe`.
3. Close your DAW.
4. Copy the whole folder `Griselda Auto Filter.vst3` to `C:\Program Files\Common Files\VST3`. Windows asks for
   administrator rights.
5. Start the DAW. In Ableton Live, open *Settings → Plug-ins*, turn on *Use VST3 Plug-in System Folders* and click
   *Rescan*.
6. Find **Griselda Auto Filter** under *Plug-ins → Schlensog Audio Lab* and drag it onto an audio track.

## A sharp picture in Ableton Live

Live can stretch plug-in windows as a picture, which makes the interface blurry. Right-click the title bar of the
Griselda device, turn off **Auto-Scale Plug-in Window**, and load the plug-in again.

The window has a fixed size of 704 × 461 points (about 1056 × 691 pixels at 150 % Windows scaling).

## The standalone application

Start `Griselda Auto Filter.exe`. Click *Options* to choose the audio device, the input and the output. The
standalone version processes the selected input live.

## The window stays empty

The interface runs in Microsoft Edge WebView2. If the window stays empty or black, install the
[WebView2 Runtime](https://developer.microsoft.com/microsoft-edge/webview2/) (the *Evergreen Bootstrapper* is enough) and
load the plug-in again.

## Updating

Close the DAW, replace the old `Griselda Auto Filter.vst3` with the new one and start the DAW again. Your settings in
saved projects stay as they were.

## Uninstalling

Delete `C:\Program Files\Common Files\VST3\Griselda Auto Filter.vst3`. Presets you exported stay where you saved them.

## macOS

There is no ready-made macOS build. The code builds with JUCE on macOS, but this is not tested; see
[docs/developer/BUILD.md](docs/developer/BUILD.md).
