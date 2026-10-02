# Speed MIDI Editor

**A simple multi-track editor for standard MIDI files.**

![Speed MIDI Editor showing a multi-track MIDI arrangement](docs/screenshots/speed-midi-editor-main.png)

*macOS development preview, captured September 27, 2026.*

Speed MIDI Editor is a community continuation of Speedy MIDI 1.1 by Holger Hoffmann. It keeps the original piano-roll workflow while bringing the application to current Qt and macOS toolchains. It is a MIDI file editor, not a digital audio workstation.

## Download

Download Speed MIDI Editor from [GitHub Releases](https://github.com/RobCZart82/Speed-MIDI-Editor/releases/tag/v0.1.3). Download files only from this repository's Releases page.

| Platform | Installer | Manual installation |
|---|---|---|
| macOS Universal 2 (Intel + Apple Silicon) | [macOS .pkg installer](https://github.com/RobCZart82/Speed-MIDI-Editor/releases/download/v0.1.3/Speed-MIDI-Editor-0.1.3-macOS-Universal.pkg) | [macOS .zip archive](https://github.com/RobCZart82/Speed-MIDI-Editor/releases/download/v0.1.3/Speed-MIDI-Editor-macOS-Universal.zip) |
| Windows x64 | [Windows Setup .exe](https://github.com/RobCZart82/Speed-MIDI-Editor/releases/download/v0.1.3/Speed-MIDI-Editor-0.1.3-Windows-x64-Setup.exe) | [Windows .zip archive](https://github.com/RobCZart82/Speed-MIDI-Editor/releases/download/v0.1.3/Speed-MIDI-Editor-Windows-x64.zip) |

The macOS installer puts **Speed MIDI Editor.app** in **Applications**. The Windows installer installs for the current user, creates a Start menu shortcut, and includes required runtimes. Manual ZIPs need no installer; extract the archive and follow the included instructions. SHA256 checksums and packaging information are attached to the release.

The macOS installer and app are not signed with a paid Apple Developer certificate, and the app is not notarized. If Gatekeeper blocks the .pkg, close the warning, open **System Settings → Privacy & Security**, scroll to **Security**, click **Open Anyway** next to the Speed MIDI Editor notice, and confirm **Open Anyway** in the next dialog. Authenticate as an administrator if asked. Reopen the installer and use the default **Macintosh HD** destination unless you need another location. Repeat approval for the installed app only if macOS blocks it separately. See Apple's [instructions](https://support.apple.com/en-gb/102445).

Windows may show SmartScreen or an unknown-publisher warning because the packages have no publisher certificate. Only continue for downloads verified as coming from this project's official Releases page. If Windows Defender reports malware, stop and do not bypass its warning.

## Features

- Multi-track piano-roll editing with note, velocity, and controller workflows
- Standard MIDI File import and export
- MIDI playback and device output, including macOS CoreMIDI destinations
- Track Solo, Mute, and Record controls, a MIDI activity indicator, Fit All Tracks, and equal-height track sizing
- Blue-gray editor theme with a blue toolbar and native macOS window controls

### Getting sound on macOS

Open **Options → Preferences → MIDI ports**. Choose **Apple Built-in General MIDI** as the **Output port** to play through the macOS system instrument, when available. The **Input port** is separate and receives MIDI from a keyboard or controller. External MIDI destinations are also supported.

## System requirements

### For users

- macOS Universal 2 supports Apple Silicon and Intel in one app. macOS 13 or newer is the planned minimum; clean-machine and minimum-OS validation is still incomplete.
- Windows x64 targets Windows 10 version 1809 or newer and Windows 11. Windows 11 ARM may run the x64 build through emulation; MIDI device and playback behavior in that setup has not been verified.
- Read the release notes for tested configurations and known limitations.

### To build from source

- CMake 3.21 or newer
- A C99/C++17-compatible compiler
- Qt 6.8 or newer with Widgets, Xml, Network, Svg, and LinguistTools components

Linux build configuration is preliminary and is not a release target. Windows x64 packaging and automated MIDI backend tests pass; Windows 11 Microsoft GM playback has also been manually confirmed. Clean-machine and minimum-OS validation remain incomplete.

## Build from source

Install Qt 6 and CMake, then configure with your Qt installation path. The release workflow uses Qt 6.10.3 and builds a Universal 2 app:

```sh
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH="/path/to/Qt/6.10.3/macos" \
  -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

For a single-architecture development build, set `CMAKE_OSX_ARCHITECTURES` to `arm64` or `x86_64` instead. The current local Universal 2 bundle contains both architectures.

## Development status

The published version is **0.1.3**. See the [0.1.3 release notes](docs/RELEASE_NOTES_0.1.3.md). CTest covers MIDI/RMID parsing, repeated save/reload, SysEx and track-end preservation, overlapping-note import, real editor actions and undo/redo, current and legacy clipboard data, MIDI lifecycle, and platform backend failures. The [contributor guide](CONTRIBUTING.md#verification) maps regression targets to their coverage. Both platform workflows run tests before deployment and package verification. The maintainer tested the 0.1.3 installers and application on Windows and macOS and reported no visible or audible faults. Detailed OS/device and individual upgrade/save-reopen results were not separately supplied; see the [0.1.3 QA log](docs/RELEASE_QA_0.1.3.md). Minimum-OS, clean-machine and external-hardware coverage remains incomplete; see the [0.1.3 release notes](docs/RELEASE_NOTES_0.1.3.md).

Run the automated tests after building:

```sh
ctest --test-dir build -C Release --output-on-failure --no-tests=error
```

The v0.1.3 release provides macOS Universal 2 and Windows x64 installers, manual ZIPs, and SHA256 checksums. Future version tags prepare draft releases for review before publication.

## About and license

The original Speedy MIDI 1.1 source snapshot is preserved in [`upstream-1.1/`](upstream-1.1/). Speed MIDI Editor and its modifications are licensed under the GNU General Public License, version 3 or any later version ([`GPL-3.0-or-later`](LICENSE)). Original author notices are retained. The upstream project is available on [SourceForge](https://sourceforge.net/projects/speedymidi/).

See [`docs/TECHNICAL_AUDIT.md`](docs/TECHNICAL_AUDIT.md) and [`src/speedymidi/images/README.images`](src/speedymidi/images/README.images) for third-party component and asset notes. Qt is a build dependency and is not included in the source repository.

See [`docs/RELEASE_READINESS.md`](docs/RELEASE_READINESS.md) for the pre-release test matrix and remaining release gates.

## Documentation

- [User guide (English)](docs/USER_GUIDE_EN.md)
- [Felhasználói útmutató (magyar)](docs/USER_GUIDE_HU.md)
- [Technical audit](docs/TECHNICAL_AUDIT.md)
- [Contributing](CONTRIBUTING.md)
- [Changelog](CHANGELOG.md)
- [Release publication plan (Hungarian)](docs/RELEASE_PUBLICATION_PLAN_HU.md)

### Installers

The macOS Universal 2 installer is a .pkg that installs Speed MIDI Editor.app into Applications. The macOS manual ZIP contains the app for manual placement. Windows offers a Setup .exe for the current user and a manual ZIP. Publisher signing and Apple notarization are not configured.

See [installer build and verification](docs/INSTALLERS.md) for packaging details.
