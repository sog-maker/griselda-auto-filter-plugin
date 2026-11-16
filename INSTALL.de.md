# Griselda Auto Filter installieren

[English](INSTALL.md)

## Was du brauchst

- Windows 10 oder 11, 64 Bit
- eine DAW, die VST3-Plug-ins lädt (getestet mit Ableton Live 12), oder keine für das Standalone-Programm
- die Microsoft Edge WebView2 Runtime (Windows 11 hat sie)

## Schritt für Schritt

1. Das [neueste Release](https://github.com/sog-maker/griselda-auto-filter-plugin/releases/latest) öffnen und das
   Windows-Zip laden.
2. Das Zip entpacken. Darin liegen der Ordner `Griselda Auto Filter.vst3` und das Programm `Griselda Auto Filter.exe`.
3. Die DAW schließen.
4. Den ganzen Ordner `Griselda Auto Filter.vst3` nach `C:\Program Files\Common Files\VST3` kopieren. Windows fragt nach
   Administratorrechten.
5. Die DAW starten. In Ableton Live unter *Einstellungen → Plug-ins* die Option *VST3-Plug-in-Systemordner verwenden*
   einschalten und auf *Neu scannen* klicken.
6. **Griselda Auto Filter** unter *Plug-ins → Schlensog Audio Lab* suchen und auf eine Audiospur ziehen.

## Ein scharfes Bild in Ableton Live

Live kann Plug-in-Fenster als Bild strecken, dann wirkt die Oberfläche unscharf. Mit der rechten Maustaste auf die
Titelleiste des Griselda-Geräts klicken, **Auto-Skalierung Plug-in-Fenster** ausschalten und das Plug-in neu laden.

Das Fenster hat eine feste Größe von 704 × 461 Punkten (bei 150 % Windows-Skalierung etwa 1056 × 691 Pixel).

## Das Standalone-Programm

`Griselda Auto Filter.exe` starten. Unter *Options* Audiogerät, Eingang und Ausgang wählen. Die Standalone-Version
bearbeitet den gewählten Eingang live.

## Das Fenster bleibt leer

Die Oberfläche läuft in Microsoft Edge WebView2. Bleibt das Fenster leer oder schwarz, die
[WebView2 Runtime](https://developer.microsoft.com/microsoft-edge/webview2/) installieren (der *Evergreen Bootstrapper*
reicht) und das Plug-in neu laden.

## Aktualisieren

Die DAW schließen, die alte `Griselda Auto Filter.vst3` durch die neue ersetzen und die DAW wieder starten. Die
Einstellungen in gespeicherten Projekten bleiben erhalten.

## Deinstallieren

`C:\Program Files\Common Files\VST3\Griselda Auto Filter.vst3` löschen. Exportierte Presets bleiben, wo du sie
gespeichert hast.

## macOS

Einen fertigen macOS-Build gibt es nicht. Der Code lässt sich mit JUCE auf macOS bauen, das ist aber nicht getestet;
siehe [docs/developer/BUILD.de.md](docs/developer/BUILD.de.md).
