# Changelog

## 0.1.0 – 2026-09-29

- Modernize the original Speedy MIDI 1.1 application for Qt 6 and CMake while retaining the upstream source snapshot and attribution.
- Add a blue-gray editor theme, original flat toolbar artwork, redesigned track controls, and improved scrollbar/zoom layout.
- Add drawing, erasing, moving, and resizing of notes, persistent editor tool selection, right-click tool deactivation, Fit All Tracks, and Default Track Height controls.
- Add macOS Apple General MIDI output support and MIDI activity indicators.
- Add English and Hungarian user guides.
- Add GitHub Actions workflows for Qt-deployed macOS Universal 2 and Windows x64 packages. Both hosted builds and package uploads now pass; clean-machine coverage remains incomplete.
- Add CTest coverage for MIDI parsing/import, clipboard, lifecycle and Windows backend failures; fix x64 output crashes and runtime deployment.
- Remove the Windows console window, correct version metadata, protect part exports, validate tempo conversion and bound corrupt clipboard lists.
- Add verified dual-platform release drafts with SHA256 checksums. Manual Windows GM and macOS playback/save/reopen checks passed on the preceding stability build; minimum-OS, clean-machine and external-hardware checks remain incomplete. See [`docs/RELEASE_READINESS.md`](docs/RELEASE_READINESS.md).
