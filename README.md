**English** | [日本語](README.ja.md) | [한국어](README.ko.md) | [Bahasa Indonesia](README.id.md) | [Français](README.fr.md) | [繁體中文](README.zh-TW.md)

# VST Ex Audio Filter for OBS

An extended audio filter window for OBS Studio with instant ON/OFF switching for live singing, latency tools
and a chain view, plus **VST Enigmax Loader**, its own **VST2 / VST3** host. The *Filters (VST Ex)* window works
on the same filter chain as OBS's own *Filters* window, so you can use either.

Two OBS plugins are installed: **VST Ex** (the window, docks and built-in filters) and **VST Enigmax Loader**
(the VST host). VST filters belong to VST Enigmax Loader: when it is removed, OBS can no longer load them. Move VST2 filters
to OBS's own VST 2.x filter first with *Hand over* (see below).

- Windows x64, OBS Studio 31 or newer (tested on 33.0)
- Author: ChromeBread feat. BlackDream · License: GPL-2.0

## Install

1. Download `OBS-VST-Ex-Audio-Filter-<version>-windows-x64-setup.exe` from [Releases](../../releases).
2. Quit OBS and run it. *Full installation* installs everything; *Custom installation* lets you choose:
   - VST Ex Audio Filter (required): *Filters (VST Ex)*, pitch / pseudo-stereo filters, Tools > #cbOBScripts > VST Ex Audio Filter
   - VST Enigmax Loader: VST2 / VST3 plugins
   - VST Ex Chain View, VST Ex Mixer dock, VST Ex Switcher dock

   The optional VST Ex parts can be switched on or off later in Tools > #cbOBScripts > VST Ex Audio Filter (applies after an OBS restart).
3. Start OBS. On first start it asks where to keep thumbnails, screenshots and history.

**Portable (no installer)**: `OBS-VST-Ex-Audio-Filter-<version>-windows-x64-portable.zip`. Quit OBS and extract it
into `C:\ProgramData\obs-studio\plugins\`: you get the folders `OBS-VST-Ex-Audio-Filter` (VST Ex) and
`vst-enigmax-loader` (VST Enigmax Loader). Leave out a folder you do not want. To remove it, quit OBS and delete the
folders (hand VST filters over first, see below).
To uninstall with a backup, run `OBS-VST-Ex-Audio-Filter\Safe_Uninstaller\Safe_Uninstaller.bat`.

**Uninstall** (Windows Settings > Apps > VST Ex Audio Filter for OBS). The uninstaller:
1. warns if your scene collections still use VST Enigmax Loader filters, so you can hand them over first;
2. asks separately whether to delete plugin thumbnails, high-resolution screenshots, and settings / presets /
   history, optionally exporting everything first;
3. lists files in its folders that it did not make (other files, renamed copies such as `.bak` / `.org`) and lets
   you delete or keep each one.

## VST Ex Audio Filter

**Filters (VST Ex) window** — Audio Mixer: right-click a source > *Filters (VST Ex)*
- Big ON/OFF buttons, multi-select, drag to reorder, duplicate (Ctrl+D), delete all, a note per filter
- Undo / Redo and an autosaved history that survives OBS restarts
- Per-filter latency, total latency, and a button that sets Sync Offset to compensate
- **Presets**: save / apply / export / import a whole filter chain
- **ON/OFF sets** with Sync Offset and volume: switch verse → chorus mid-song instantly, by hotkey
- **Measure with 4 beats**: a BPM120 test song plays, you clap along (or hold an earphone to the mic) and the Sync Offset is computed; zoomable waveforms with a millisecond ruler for fine manual adjustment

<p><img src="docs/images/preview-filters.png" alt="Filters (VST Ex) window" width="49%"> <img src="docs/images/preview-filters-select.png" alt="Filters (VST Ex) window, several filters selected" width="49%"></p>

*Filters (VST Ex) window / Filters (VST Ex) window, several filters selected*

<p><img src="docs/images/preview-history.png" alt="History" width="49%"> <img src="docs/images/preview-sync.png" alt="Sync Offset from 4 beats" width="49%"></p>

*History / Sync Offset from 4 beats*

**Built-in filters**
- *VST Ex Pitch / Pan*: pitch in semitones and pan, handy to tell devices apart when aligning them
- *VST Ex Pseudo Stereo*: widens a mono mic without a DAW (Haas effect), one filter to add or remove

- Tools > #cbOBScripts > VST Ex Audio Filter: language (EN / JP / KR / ID / FR / TW), version, optional parts, **Debug mode**

## VST Enigmax Loader

- VST2 and VST3, crash-safe scanning in a separate process (started by you, with time estimate)
- Browser with list / thumbnails / details view, sort, search, favorites, HID (hide) marks, notes, recently used
- Thumbnails taken automatically the first time an editor opens, or all at once; per-filter screenshots
- Export / import plugin settings from the editor window
- Thumbnails and high-resolution screenshots are kept in separate folders
- Copies of a plugin in different folders are separate entries, as in OBS: favorites, HID, notes and thumbnails are set one by one

![Choose a VST Plugin (thumbnail view)](docs/images/preview-browser-thumbnails.png)
*Choose a VST Plugin (thumbnail view)*

<p><img src="docs/images/preview-browser-list.png" alt="Choose a VST Plugin (list view)" width="49%"> <img src="docs/images/preview-browser-details.png" alt="Choose a VST Plugin (details view)" width="49%"></p>

*Choose a VST Plugin (list view) / Choose a VST Plugin (details view)*

## VST Ex Chain View

- *VST Ex Chain View* (Tools menu): every source from device to output, latency, levels, sidechain links, optional half-circle imager, save as image
- Choose what the view and the saved image show: active sources only, tracks 1 to 6, and volume, mono, balance, Sync Offset and Audio Monitoring one by one
- Drag rows to reorder sources, right-click a row to write a memo next to the source name, start a new line every N VSTs

![VST Ex Chain View](docs/images/preview-chain.png)
*VST Ex Chain View*

## VST Ex Mixer

- *VST Ex Mixer* dock: one column per source under the Audio Mixer's meters, a button per VST

![OBS with the VST Ex Mixer and VST Ex Switcher docks](docs/images/preview-obs.png)
*OBS with the VST Ex Mixer and VST Ex Switcher docks*

## VST Ex Switcher

- *VST Ex Switcher* dock: ON/OFF set buttons and CPU usage (OBS total and VST share)

## VST Ex WaveLine

- Waveforms of the sources you choose, flowing on top of each other
- Bar on the left: zoom (2 to 60 seconds shown). Tr buttons on the right: sources on the lit tracks that reach the stream are strong, the others faint. The buttons only change the display, not the routing
- `●`: beat dots at the BPM you set (default 120); press it on a beat to line them up. `≡`: layer order and opacity of each source

## Build

Visual Studio 2022 and CMake 3.28+: `cmake --preset windows-x64` then
`cmake --build build_x64 --config RelWithDebInfo`.

VST is a registered trademark of Steinberg Media Technologies GmbH.

## Updates

This software does not check for updates or update itself, and no automatic update feature that connects to the internet is planned. To update, follow the developer's link and download the new version from the distribution site yourself: [Releases](../../releases)
