# Contributing

Speed MIDI Editor is a community continuation of Speedy MIDI 1.1. Keep changes focused on Standard MIDI File editing, reliability, accessibility, and the current macOS-first release target. Preserve upstream copyright notices and third-party license terms when changing inherited files.

## Build

The project uses CMake 3.21+, C99/C++17, and Qt 6 with Widgets, Xml, Network, Svg, and LinguistTools. Release workflows pin Qt 6.10.3. The macOS workflow builds `x86_64;arm64` as a single Universal 2 package; the Windows workflow builds x64 with MSVC 2022. Linux remains preliminary.

```sh
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH="/path/to/Qt/6.10.3/macos" \
  -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Verification

CTest runs automated MIDI file, playback, platform backend, editor-state and local-instance message regressions. Run `ctest --test-dir build --output-on-failure --no-tests=error` after building (add `-C Release` for MSVC). Tests use fake MIDI output and do not need an attached instrument; the separate Windows device smoke test may skip when no device is available. GUI editing tests use the offscreen Qt platform and temporary preferences.

For Clang/GCC, configure a separate Debug build with `-DSPEED_MIDI_SANITIZERS=ON`. Run tests with `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` and `ASAN_OPTIONS=halt_on_error=1:detect_leaks=0`; any sanitizer diagnostic must fail the run. The macOS workflow runs this check in addition to the release package build.

Before changing MIDI parsing, editing, playback, or device code, add a regression for the failing behavior and run the relevant manual hardware checks in [`docs/RELEASE_READINESS.md`](docs/RELEASE_READINESS.md). Automated tests do not establish timing accuracy on a physical MIDI device. Report OS, architecture, Qt version, build type, exact steps, and a representative MIDI file when filing a defect.

## Scope and provenance

Prefer the smallest change that solves a concrete correctness or usability issue. Keep editor behavior consistent with Standard MIDI Files and the documented tool modes. Preserve `upstream-1.1/` as an untouched provenance snapshot. Do not present preliminary Windows/Linux configuration as a supported release platform until it has been built and exercised on those systems.
