# Speed MIDI Editor

A lightweight, multi-track Standard MIDI File editor based on Speedy MIDI 1.1.

Speed MIDI Editor aims to preserve Speedy MIDI's simple editing workflow while bringing its code and builds to current platforms. It remains a MIDI editor, not a DAW: the project does not target audio tracks, plug-in hosting, mixing, or other studio features.

The original Speedy MIDI 1.1 source snapshot is preserved in [`upstream-1.1/`](upstream-1.1/), with the `upstream-1.1` Git tag intended to identify that unchanged snapshot. Original attribution and GPLv3 terms are retained in [`LICENSE`](LICENSE). The application is a community fork based on work by Holger Hoffmann; original project: [SourceForge Speedy MIDI](https://sourceforge.net/projects/speedymidi/).

## License

Speed MIDI Editor and its modifications are licensed under the GNU General Public License, version 3 or any later version (`GPL-3.0-or-later`). The original author notices are retained. See [`LICENSE`](LICENSE) for the full GPLv3 text. Some bundled third-party assets and components have their own notices and terms; see the provenance notes in [`docs/TECHNICAL_AUDIT.md`](docs/TECHNICAL_AUDIT.md) and [`src/speedymidi/images/README.images`](src/speedymidi/images/README.images).

## Development status

This repository is at version **0.1.0** and is in the Qt 6 porting phase. The project configures and builds on macOS with Qt 6.11.2 for both Apple Silicon (`arm64`) and Intel (`x86_64`). The app launches and opens the included multi-track MIDI example. Playback, editing and save/reopen behavior, MIDI device I/O, and release packaging still need verification.

## Build prerequisites

- CMake 3.21 or newer
- A C99/C++17 compiler
- Qt 6.2 or newer: Widgets, Xml, Network, Svg, and LinguistTools components
- macOS: Xcode Command Line Tools; CoreMIDI/CoreFoundation/CoreAudio are provided by macOS
- Windows: a supported Qt 6 toolchain and Windows SDK
- Linux (preliminary backend wiring): ALSA development files and `pkg-config`

Configure and build on macOS with (adjust the Qt path and architecture for your machine):

```sh
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH="/path/to/Qt/6.x/macos" \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

For Intel, use `-DCMAKE_OSX_ARCHITECTURES=x86_64`. The currently verified Qt 6.11.2 macOS kit contains both architectures. Universal 2 packaging is a later milestone and requires matching Qt and dependency binaries for both architectures.

The first release targets macOS Apple Silicon. Intel macOS and Universal 2 follow after that target is stable; modern Windows support follows macOS. The current CMake file expresses early macOS and Windows backend selection, not completed or verified platform support.

The editor uses a blue-gray graphite palette for its editing surfaces, with an Office 2007 Blue-inspired menu bar and light-blue toolbar. The operating system keeps its native window controls. The zoom controls sit beside the lower-right ends of the scrollbars, whose handles and arrow buttons use a darker blue for contrast. Existing MIDI files retain their stored per-track note colors; new tracks use a muted set of defaults.

CMake compiles the existing `.ts` catalogs and copies their `.qm` files beside the executable, matching the inherited runtime translation lookup.

GitHub Actions is configured to install Qt 6 and compile separate arm64 and x86_64 builds on macOS for each push and pull request. The workflow uploads the resulting `.app` bundles as build artifacts; these are CI outputs, not yet self-contained release packages. The workflow can only be confirmed after the repository is pushed to GitHub.

## Scope

- Multi-track MIDI editing and piano-roll workflow
- Note, velocity, and controller editing
- Standard MIDI File import and export
- MIDI playback and device I/O
- Keep the existing workflow recognizable and avoid feature creep

See [`docs/TECHNICAL_AUDIT.md`](docs/TECHNICAL_AUDIT.md) for the source and dependency audit and [`CONTRIBUTING.md`](CONTRIBUTING.md) for the porting sequence.

## Third-party components and assets

The supplied source includes an old, modified PortMidi implementation. It remains vendored for the initial port; its license is included in [`third_party/portmidi/LICENSE.PortMidi`](third_party/portmidi/LICENSE.PortMidi). The Qt Solutions single-application/locked-file sources contain Nokia/Qt Solutions licensing notices that must be reconciled with their included GPLv3 alternative before distribution. Image attribution and asset terms are recorded in `src/speedymidi/images/README.images`; review that inventory before making a release. In particular, keep the Aha-Soft music-library icon's stated incorporation restriction in view.

Qt itself is a build dependency and is not copied into this repository. Distributions must separately comply with the selected Qt version's license and packaging requirements.
