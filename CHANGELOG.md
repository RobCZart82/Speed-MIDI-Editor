# Changelog

## Unreleased

- Add release asset hash, ZIP integrity and exact build-provenance verification before draft uploads, with failure regressions.

## 0.1.5 – release candidate, unpublished

- Reject overflowing mouse draw/move/resize intervals before changing notes or undo history.
- Display markers at their exact imported timestamps and navigate several markers within one grid cell without invalid editor selections.
- Keep beat borders consistent at odd PPQN resolutions with fractional beat lengths.
- Correct English/Hungarian download and installer guidance; retire unsupported qmake projects with a CMake migration message.
- Includes the fixes listed under the superseded 0.1.4 candidate below.

## 0.1.4 – superseded, unpublished draft

- Reject overflowing measure/cell insertions and note-length scaling before creating undo commands; preserve the document and undo history on rejection.
- Refuse malformed or zero SMF tempo payloads with an import error instead of losing or truncating tempo data.
- Preserve exact conductor marker timestamps, same-tick packet order and original bytes through save/reopen and V5 whole-measure clipboard operations. Text edits replace original packets; color-only edits preserve them. Older clipboard formats remain available with documented limitations.
- Release sustain, sostenuto and Hold2 pedals after queued output when muting or pausing playback.
- Encode CoreMIDI endpoint names safely in UTF-8, release owned names and CoreFoundation strings, and roll back devices/default IDs after partial enumeration failure.
- Release the Windows instance mutex and make WinMM device identifier pointer casts explicit.
- Resolve annotated release tags to commits and distinguish missing tags from API failures before creating a tag.
- Expand numeric-edit, tempo, marker/clipboard, pedal and native CoreMIDI failure regressions. Installer formats remain Windows EXE/macOS PKG plus manual ZIPs.

## 0.1.3 – 2026-10-02

- Preserve exact key-signature timestamps, including within a measure, through saving and whole-measure clipboard operations.
- Use whole-measure arithmetic for odd-PPQN clipboard validation before and after resolution conversion; reject fractional-tick source or destination grids without changing the document or undo history.
- Limit key signatures to the standard SMF range of seven flats through seven sharps consistently in import, export, the properties dialog, the model and clipboard validation.

- Prevent fractional-tick meter edits before changing the document or undo history, and refuse export of meter grids that cannot be reopened.
- Refuse time or key signatures on ordinary Format 1 tracks with an explanation instead of silently discarding them. Keep the existing document and source file unchanged on failed open.

## 0.1.2 – superseded, unpublished draft

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

