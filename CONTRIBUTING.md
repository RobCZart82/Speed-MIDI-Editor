# Contributing

Speed MIDI Editor is a community continuation of Speedy MIDI 1.1. Keep changes focused on Standard MIDI File editing, reliability, accessibility, and the current macOS-first release target. Preserve upstream copyright notices and third-party license terms when changing inherited files.

## Build

The project uses CMake 3.21+, C99/C++17, and Qt 6.2 or newer with Widgets, Xml, Network, Svg, and LinguistTools. For macOS, configure with the path to the Qt macOS kit and select `arm64` or `x86_64` using `CMAKE_OSX_ARCHITECTURES`; the GitHub workflow currently builds both architectures separately.

```sh
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH="/path/to/Qt/6.x/macos" \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Verification

There is currently no automated unit or integration test target (`ctest` reports zero tests). A successful build proves compilation and linking only. Before changing MIDI parsing, editing, playback, or device code, add focused tests where practical and run the manual checks in [`docs/RELEASE_READINESS.md`](docs/RELEASE_READINESS.md). Report OS, architecture, Qt version, build type, exact steps, and a representative MIDI file when filing a defect.

## Scope and provenance

Prefer the smallest change that solves a concrete correctness or usability issue. Keep editor behavior consistent with Standard MIDI Files and the documented tool modes. Preserve `upstream-1.1/` as an untouched provenance snapshot. Do not present preliminary Windows/Linux configuration as a supported release platform until it has been built and exercised on those systems.
