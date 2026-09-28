# Changelog

## 0.1.0 – Unreleased

- Modernize the original Speedy MIDI 1.1 application for Qt 6 and CMake while retaining the upstream source snapshot and attribution.
- Add a blue-gray editor theme, original flat toolbar artwork, redesigned track controls, and improved scrollbar/zoom layout.
- Add drawing, erasing, moving, and resizing of notes, persistent editor tool selection, right-click tool deactivation, Fit All Tracks, and Default Track Height controls.
- Add macOS Apple General MIDI output support and MIDI activity indicators.
- Add English and Hungarian user guides.
- Build the macOS application in Release mode for Apple Silicon and Intel; GitHub Actions provides separate architecture build artifacts for development.
- Automated tests, broad MIDI round-trip coverage, deployed Qt application packaging, and release-candidate runtime checks are still outstanding. See [`docs/RELEASE_READINESS.md`](docs/RELEASE_READINESS.md).
