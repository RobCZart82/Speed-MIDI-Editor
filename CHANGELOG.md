# Changelog

## Unreleased

## 0.1.2 – draft preparation

- Reject MIDI files with mid-measure meter changes or fractional-tick measure lengths instead of silently discarding meter events. Show the reason and retain the current document and original file.
- Reject out-of-range or malformed meters and invalid key signatures with the same safe-open behavior.
- Preserve imported MIDI metronome and notation bytes through saves, measure editing, copy/paste and undo/redo. Retain compatibility with earlier clipboard formats.
- Clarify current Windows/macOS distribution and keep earlier release QA evidence explicitly historical.

## 0.1.1 – 2026-10-01

- Preserve opaque SysEx packets, unmatched notes, track endpoints, release velocity, source MIDI event order, bank/program setup and Unicode metadata through repeated saves.
- Preserve exact 24-bit MIDI tempo values, fractional BPM and changes within a measure; accumulate precise playback timing.
- Harden clipboard validation and resolution conversion, note editing and undo/redo, part export paths, input recovery, queued playback and state restoration.
- Add double-click and single-click hints to track setup and Cell length fields; improve About text/license layout.

- Reject out-of-range measure rebars before changing the document or undo history; use 64-bit position arithmetic and explain rejected changes in the measure dialog.
- Release sostenuto (CC66) and Hold 2 (CC69) alongside sustain when disabling MIDI Thru or recovering from input loss; retain held-controller state after All Sound Off.

- Preserve valid meter changes at odd MIDI PPQN resolutions by rounding only the complete measure length, with repeated-save coverage through the maximum SMF PPQN.

- Build and verify a macOS PKG installer instead of DMG, including installation and reinstallation checks; preserve the portable ZIP.

- Preserve initial SysEx reset and patch/volume/pan event order through repeated MIDI saves; keep later track-property edits effective in export and playback.
- Release forwarded notes and sustain when MIDI Thru is disabled, with targeted note-offs rather than a channel-wide reset during a normal toggle.
- Restore the missing F/E horizontal piano roll boundaries, with rendering coverage across zoom, fractional centers and 100–200% display scales.
- Document and regression-test the bounded queued-output delay after Stop, Pause and Mute. Already submitted events are not canceled per track.

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
