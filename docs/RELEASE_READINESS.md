# First release readiness

Status checked against the current `main` source and a local macOS Qt 6.11.2 Release build. This document separates existing checks from work still required; checklist items are not evidence that a test has already passed.

## Current automated coverage

| Area | Current coverage | Readiness |
|---|---|---|
| macOS compilation | GitHub Actions builds macOS arm64 and x86_64 separately and uploads each `.app` as a workflow artifact. A local Universal 2 Release build also succeeds. | Build coverage exists; inspect the latest workflow run before each release. |
| Unit/integration tests | No test target is registered; `ctest --test-dir build-test-release -N` reports `Total Tests: 0`. | Gap. MIDI parsing/editing/playback regressions are not automatically caught. |
| GUI behavior | Key editing tools, playback, and Apple General MIDI output have been manually tried during development. | Manual spot checks only; repeat on a release candidate and cover workflows below. |
| Static analysis | No `clang-tidy`, `cppcheck`, or `scan-build` step is configured. | Not covered. |
| App distribution | CI uploads the built `.app` directory. It does not run `macdeployqt`, sign/notarize, create a DMG, or test a copied app on a clean Mac. | Not a user-ready standalone download yet. Decide and implement the chosen GitHub-only delivery format before publishing. |
| Other platforms | Windows/Linux build wiring is present but not validated by CI here. | Do not claim support in the first macOS release. |

## Required release-candidate checks

Run these against a tagged release candidate, record macOS version/architecture and outcome, and attach representative MIDI files where useful.

### MIDI file integrity

- [ ] Open valid Format 0 and Format 1 Standard MIDI Files; verify track names, notes, tempo, meter, key signature, markers, and multiple channels.
- [ ] Save As to a new file, close, reopen, and compare event content and timing in the editor and a second MIDI reader.
- [ ] Confirm format conversion/export options and editor metadata behavior do not corrupt ordinary MIDI events.
- [ ] Try a large, long-duration file and files with unusual tempo/meter changes.
- [ ] Try empty, truncated, malformed, and unsupported files; expect a clear error and no crash or partial overwrite.

### Editing and undo

- [ ] Draw a note, erase it, move it in time and pitch, and resize both ends; verify snapping and boundaries.
- [ ] Confirm tool buttons stay active, switching tools works, and right-click turns the active hand/pencil/eraser tool off as described in both guides.
- [ ] Verify undo/redo after each edit type, and confirm edits persist after save/reopen.
- [ ] Check copy/paste, cut, selection, track add/remove, Solo/Mute/Record state, and per-track properties.
- [ ] Exercise Fit All Tracks and Default Track Height at several window sizes and with many tracks/measures.

### Playback and MIDI devices

- [ ] Play, pause/stop, and return to the beginning; check cursor visibility and scrolling near song end.
- [ ] Verify Apple Built-in General MIDI playback through the selected macOS MIDI output.
- [ ] Verify at least one external MIDI destination when available; test device disconnect/reconnect and missing-device startup.
- [ ] Confirm MIDI activity indicators respond and all notes stop cleanly when playback stops or a device changes.

### App lifecycle and distribution

- [ ] Launch from a clean macOS user account or clean test Mac without a developer Qt installation; open and save a MIDI file.
- [ ] Confirm the packaged app contains the required Qt frameworks, plugins, translations, and app resources.
- [ ] Verify first launch, reopening a file through Finder, preferences persistence, app quit/relaunch, and single-instance behavior.
- [ ] Test minimum macOS version and both Apple Silicon and Intel, if both are claimed.
- [ ] Review app name/version, About dialog, screenshot, README download instructions, and release notes.
- [ ] Confirm the GitHub release asset/source archive includes GPLv3 text, upstream notices, PortMidi notice, Qt Solutions source notices, and a third-party asset inventory.

## Release blockers found in this audit

1. **No automated tests.** Add at least parser/import-export and core edit-model regression tests in a follow-up, then register them with CTest and CI.
2. **No deployed app package.** Current CI artifacts are build-tree `.app` bundles and have not been verified on a Mac without Qt installed. The selected download method needs packaging and clean-machine verification.
3. **Runtime/file QA is incomplete.** Manual user trials cover important features but do not document the full matrix above or round-trip correctness across representative files.
4. **Warnings remain.** The local build reports deprecated Carbon APIs and `MAXPATHLEN` redefinition in vendored PortMidi, plus deprecated Qt APIs and enum mismatches. Track these as technical debt and review the CoreMIDI preference lookup path on supported macOS versions.

No release has been published by this audit. Keep the version marked unreleased until the release gates above have owners and results recorded.
