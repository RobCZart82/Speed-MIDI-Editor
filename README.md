# Speed MIDI Editor

**A lightweight, multi-track editor for Standard MIDI Files.**

![Speed MIDI Editor showing a multi-track MIDI arrangement](docs/screenshots/speed-midi-editor-main.png)

*macOS development preview, captured September 27, 2026.*

Speed MIDI Editor is a community continuation of Speedy MIDI 1.1 by Holger Hoffmann. It keeps the original piano-roll workflow while bringing the application to current Qt and macOS toolchains. It is a MIDI file editor, not a digital audio workstation.

## Download

There is no official release yet. The [GitHub Actions page](https://github.com/RobCZart82/Speed-MIDI-Editor/actions) is where macOS development build artifacts will appear after successful workflow runs. These temporary artifacts are for testing; they are not signed or notarized releases.

Because the planned macOS download is not signed or notarized, macOS may warn on first launch. Only use a package downloaded from this project's GitHub Releases page. To open it, Control-click **Speed MIDI Editor.app**, choose **Open**, then confirm **Open** in the dialog. Apple documents this one-time override in [Open a Mac app from an unknown developer](https://support.apple.com/en-am/guide/mac-help/open-a-mac-app-from-an-unknown-developer-mh40616/mac). This step is not needed for a future signed and notarized build.

## Features

- Multi-track piano-roll editing with note, velocity, and controller workflows
- Standard MIDI File import and export
- MIDI playback and device output, including macOS CoreMIDI destinations
- Track Solo, Mute, and Record controls, a MIDI activity indicator, Fit All Tracks, and equal-height track sizing
- Blue-gray editor theme with a blue toolbar and native macOS window controls

## System requirements

### For users

- macOS Universal 2 is the primary release target: one app for Apple Silicon and Intel.
- The planned minimum macOS version is macOS 13 or newer, matching the Qt 6.10 supported runtime target. This still needs clean-machine verification before release.
- Windows x64 is also being prepared for Windows 10 version 1809 or newer and Windows 11. Windows 11 ARM may run this build through x64 emulation; that configuration needs device/playback testing.
- Downloadable packages are still being prepared and are not official releases.

### To build from source

- CMake 3.21 or newer
- A C99/C++17-compatible compiler
- Qt 6.2 or newer with Widgets, Xml, Network, Svg, and LinguistTools components

Linux build configuration is preliminary and is not a release target. Windows x64 is an additional planned target; its build and runtime support are not confirmed until its CI build and release-candidate checks pass.

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

The project is at version **0.1.0** and remains in active development. A local macOS Release build and a local Universal 2 app bundle exist. The first GitHub Actions attempts stopped while installing Qt, before compiling; the workflows now use Qt 6.10.3 and select the required Qt archives, and reruns are pending. Interactive editing, the new tool modes, and Apple General MIDI playback have been tried during development, but there is not yet an automated test suite. Save/reopen compatibility, broad MIDI file coverage, and clean-machine application packaging still need release-candidate verification.

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
