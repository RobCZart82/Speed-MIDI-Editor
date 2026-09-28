# Speed MIDI Editor

**A lightweight, multi-track editor for Standard MIDI Files.**

![Speed MIDI Editor showing a multi-track MIDI arrangement](docs/screenshots/speed-midi-editor-main.png)

*macOS development preview, captured September 27, 2026.*

Speed MIDI Editor is a community continuation of Speedy MIDI 1.1 by Holger Hoffmann. It keeps the original piano-roll workflow while bringing the application to current Qt and macOS toolchains. It is a MIDI file editor, not a digital audio workstation.

## Download

There is no official release yet. The [GitHub Actions page](https://github.com/RobCZart82/Speed-MIDI-Editor/actions) is where macOS development build artifacts will appear after successful workflow runs. These temporary artifacts are for testing; they are not signed or notarized releases.

## Features

- Multi-track piano-roll editing with note, velocity, and controller workflows
- Standard MIDI File import and export
- MIDI playback and device output, including macOS CoreMIDI destinations
- Track Solo, Mute, and Record controls, a MIDI activity indicator, Fit All Tracks, and equal-height track sizing
- Blue-gray editor theme with a blue toolbar and native macOS window controls

## System requirements

### For users

- macOS is the first release target. Apple Silicon and Intel builds are being prepared.
- The minimum supported macOS version has not been established yet.
- A self-contained downloadable app is not available yet, so final end-user runtime requirements will be confirmed after packaging and clean-Mac testing.

### To build from source

- CMake 3.21 or newer
- A C99/C++17-compatible compiler
- Qt 6.2 or newer with Widgets, Xml, Network, Svg, and LinguistTools components

The source contains preliminary Windows and Linux build configuration, but those platforms are not verified or supported release targets yet.

## Build from source

Install Qt 6 and CMake, then configure with your Qt installation path and desired macOS architecture:

```sh
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH="/path/to/Qt/6.x/macos" \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Use `-DCMAKE_OSX_ARCHITECTURES=x86_64` for Intel. The current macOS build has been compiled for both Apple Silicon and Intel with Qt 6.11.2.

## Development status

The project is at version **0.1.0** and remains in active development. The latest macOS Release configuration builds locally for Apple Silicon and Intel (Universal 2); GitHub Actions builds each architecture separately. Interactive editing, the new tool modes, and Apple General MIDI playback have been tried during development, but there is not yet an automated test suite. Save/reopen compatibility, broad MIDI file coverage, and clean-machine application packaging still need release-candidate verification.

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
