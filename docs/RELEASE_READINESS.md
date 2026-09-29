# First release readiness

**Current follow-up:** [release audit](RELEASE_AUDIT_2026-09-29.md) and [release notes](RELEASE_NOTES_0.1.0.md). The detailed matrix below is retained as historical coverage inventory; maintainer Windows GM playback and macOS playback/save/reopen are now confirmed on the preceding stability build. Exact macOS system/architecture and clean-machine coverage remain unspecified.

Updated stability evidence and PR dispositions: [2026-09-29 audit](STABILITY_AUDIT_2026-09-29.md). The older package links below are historical; use the replacement PR's CI results for these fixes.

Status checked against the current release-preparation working copy and a local macOS Qt 6.11.2 Release build. This document separates existing checks from work still required; checklist items are not evidence that a test has already passed.

## Current automated coverage

| Area | Current coverage | Readiness |
|---|---|---|
| macOS compilation and packaging | A local Qt 6.11.2 Universal 2 Release build succeeds. GitHub Actions using Qt 6.10.3 passed build, Qt deployment, x86_64/arm64 checks, and ZIP packaging. The downloaded artifact was independently inspected; both app binary and QtCore contain x86_64 and arm64, with runtime files and required notices/guides present. | Clean-machine launch and MIDI playback checks on Intel and Apple Silicon remain pending. |
| Unit/integration tests | CTest covers SMF/RMID parsing, note pairing, MIDI stream lifecycle and Windows x64 WinMM failures; both package workflows run it. | Interactive editing, audible playback and device hot unplug still require QA. |
| GUI behavior | Key editing tools, playback, and Apple General MIDI output have been manually tried during development. | Manual spot checks only; repeat on a release candidate and cover workflows below. |
| Static analysis | No `clang-tidy`, `cppcheck`, or `scan-build` step is configured. | Not covered. |
| App distribution | The macOS workflow deploys Qt libraries/plugins and creates an unsigned, unnotarized ZIP with the app, bilingual guides, GPL/PortMidi notices, and image attribution inventory. The Windows x64 workflow deploys Qt and creates a ZIP with the same documentation/notices. Both hosted workflows passed; both artifacts have been downloaded and structurally inspected. | Clean-machine launch and playback verification remains pending. The bilingual guides and README explain first launch for the GitHub-only unsigned macOS build. |
| Windows x64 | Qt 6.10.3/MSVC 2022 build, runtime deployment, legal/user documentation inclusion, and ZIP packaging passed in GitHub Actions. The artifact contains the executable, Qt libraries/plugins/translations, license notices, README, image attribution inventory, and both user guides. | Launch and MIDI port/playback checks on Windows 10/11 remain pending. |
| Other platforms | Linux build wiring is preliminary and not part of the agreed first-release target. | Do not claim Linux support. |

## Required release-candidate checks

Run these against a tagged release candidate, record OS version/architecture and outcome, and attach representative MIDI files where useful.

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
- [ ] Test Windows 10 1809+ and Windows 11 x64, including MIDI output enumeration and playback.
- [ ] Test the Windows x64 package under Windows 11 ARM emulation and check MIDI output compatibility before documenting it as supported.
- [ ] Review app name/version, About dialog, screenshot, README download instructions, and release notes.
- [ ] Confirm the GitHub release asset/source archive includes GPLv3 text, upstream notices, PortMidi notice, Qt Solutions source notices, and a third-party asset inventory.

## Release blockers found in this audit

1. **Broader regression coverage remains.** Parser/import and MIDI backend tests are registered with CTest and CI; full editing/undo and real external-device coverage remain.
2. **Clean-machine distribution checks remain.** GitHub Actions has produced and passed structural checks for macOS Universal 2 and Windows x64 artifacts. Confirm each package launches without a developer Qt installation and verify MIDI playback on supported operating systems before publishing a release.
3. **Runtime/file QA is incomplete.** Manual user trials cover important features but do not document the full matrix above or round-trip correctness across representative files.
4. **Warnings remain.** The local build reports deprecated Carbon APIs and `MAXPATHLEN` redefinition in vendored PortMidi, plus deprecated Qt APIs and enum mismatches. Track these as technical debt and review the CoreMIDI preference lookup path on supported macOS versions.

The Qt 6.10 target supports macOS 13 and later and Windows 10 version 1809 or later. The local development build uses Qt 6.11.2, but the hosted release workflow uses Qt 6.10.3 because the currently used Qt installer action cannot resolve Qt 6.11 package metadata. The current successful GitHub Actions artifacts are [macOS Universal 2](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/36516250896) and [Windows x64](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/36516248721); they are CI artifacts, not published Releases. No release has been published by this audit. Keep the version marked unreleased until the remaining gates above are completed.
