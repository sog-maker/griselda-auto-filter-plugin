# Third-party licences

Griselda Auto Filter is licensed under the [GNU Affero General Public License, version 3 only](LICENSE). The licence
texts of the parts below are in [`licenses/`](licenses/).

## Built into the plug-in or needed to build it

| Component | Version | Source | Licence | In this repository |
| --- | --- | --- | --- | --- |
| JUCE | 8.0.13 | https://github.com/juce-framework/JUCE | AGPLv3 or a commercial JUCE licence | no, cloned at build time; [licence](licenses/JUCE-LICENSE.md) |
| VST3 SDK | part of JUCE | https://github.com/steinbergmedia/vst3sdk | MIT | no; [licence](licenses/VST3-SDK-LICENSE.txt) |
| Microsoft WebView2 SDK | 1.0.3485.44 | https://www.nuget.org/packages/Microsoft.Web.WebView2 | Microsoft licence | no; [licence](licenses/WebView2-LICENSE.txt), [notice](licenses/WebView2-NOTICE.txt) |
| React, ReactDOM, Scheduler | 18.3.1 / 18.3.1 / 0.23.2 | https://github.com/facebook/react | MIT | no, installed by pnpm |
| Other npm packages (build only) | see the list | see the list | see the list | no, installed by pnpm; [full list](licenses/NPM-DEPENDENCIES.md) |

## Fonts

Both fonts are embedded in the plug-in as WOFF2 files from `ui/src/styles/fonts/`.

| Font | Version | Copyright | Source | Licence |
| --- | --- | --- | --- | --- |
| Jost | 3.710 | © 2020 The Jost Project Authors | https://github.com/indestructible-type/Jost | [SIL OFL 1.1](licenses/OFL-Jost.txt) |
| Poiret One | 1.001 | © 2011 Denis Masharov (The Poiret One Project Authors) | https://github.com/alexeiva/poiretone | [SIL OFL 1.1](licenses/OFL-PoiretOne.txt) |

## Interface

`ui/src/styles/theme.css` is based on the default theme of [shadcn/ui](https://ui.shadcn.com/), MIT licence,
Copyright (c) 2023 shadcn.
