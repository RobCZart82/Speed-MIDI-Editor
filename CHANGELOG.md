# Changelog

## 0.1.0 – Unreleased

- Modernize the original Speedy MIDI 1.1 application for Qt 6 and CMake while retaining the upstream source snapshot and attribution.
- Add a blue-gray editor theme, original flat toolbar artwork, redesigned track controls, and improved scrollbar/zoom layout.
- Add drawing, erasing, moving, and resizing of notes, persistent editor tool selection, right-click tool deactivation, Fit All Tracks, and Default Track Height controls.
- Add macOS Apple General MIDI output support and MIDI activity indicators.
- Add English and Hungarian user guides.
- Add GitHub Actions workflows for Qt-deployed macOS Universal 2 and Windows x64 packages. Both hosted builds and package uploads now pass; clean-machine launch and MIDI playback checks remain outstanding.
- Automated tests, broad MIDI round-trip coverage, Windows runtime validation, and release-candidate checks are still outstanding. See [`docs/RELEASE_READINESS.md`](docs/RELEASE_READINESS.md).
