# Contributing

Speed MIDI Editor keeps Speedy MIDI's lightweight Standard MIDI File editing workflow. Keep changes focused on compatibility, correctness, and the existing MIDI editing experience.

## Porting order

1. Make the application compile with Qt 6, replacing removed APIs with their current equivalents while retaining the `.ui` files and existing interaction model.
2. Bring up and verify macOS arm64, including file open/save, multi-track editing, playback, and MIDI I/O.
3. Stabilize macOS, then build and verify x86_64 and Universal 2 bundles.
4. Continue with Windows x86_64 and ARM64 after the macOS port is stable.

Prefer the smallest change that solves a concrete build or behavior problem. Preserve upstream provenance and attribution when modifying inherited files. Do not add DAW-oriented capabilities such as audio tracks, plug-in hosting, a mixer, or mastering tools.

## Build status

The initial CMake project requires CMake 3.21+, C++17, and Qt 6.2+ modules Widgets, Xml, and Network. The inherited application has not yet been ported fully to Qt 6, so a successful build is not currently claimed. Include the target OS, architecture, Qt version, and compiler when reporting build issues.
