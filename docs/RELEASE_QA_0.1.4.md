# 0.1.4 candidate QA

Unpublished candidate. Updated 9 October 2026. The previous 0.1.3 maintainer trial does not establish outcomes for these new binaries. Record PASS, FAIL or NOT RUN, with exact commit/run, OS, architecture and MIDI device.

| Gate | Status | Evidence / next action |
| --- | --- | --- |
| Correctness fixes PR48 and merged main | PASS | Main `5ad5faf27280b51d36b1ef8714945dc0dd9b182c`; Windows [37906265741](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/37906265741), macOS [37906265769](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/37906265769), Linux [37906265755](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/37906265755) |
| Version/documentation preparation PR and merged-main checks | NOT RUN | Record the preparation PR and final main commit after checks pass |
| Exact-commit draft release build | NOT RUN | Dispatch `release.yml` from main with `v0.1.4` only after preparation checks pass |
| Download hashes and ZIP integrity | NOT RUN | Check all four files against SHA256SUMS; test both ZIP CRCs |
| Package provenance, version and dependencies | NOT RUN | BUILD_INFO must match the immutable tag commit and Qt version; Windows x64 GUI/runtime/version; macOS Universal 2/bundle version/PKG payload/signature |
| Actual Windows EXE install and 0.1.3 upgrade | NOT RUN | Run on the maintainer's test system and record outcome; automated disposable-runner installation is a separate result |
| Actual macOS PKG install and 0.1.3 upgrade | NOT RUN | Record macOS version and Intel/Apple Silicon; actual launch, audible playback and save/reopen |
| Manual ZIP launch without development Qt | NOT RUN | Test Windows ZIP and macOS app ZIP separately |
| Editing/file integrity and targeted audit cases | NOT RUN | Complete the checklist below against the exact draft binaries |
| Publication approval | NOT GIVEN | Keep draft until package/manual QA is recorded and the maintainer approves publication |

## Maintainer checklist

- [ ] Windows: EXE install/upgrade, launch, output enumeration, audible GS Wavetable playback, quit/relaunch and preferences.
- [ ] macOS: PKG install/upgrade, launch, available GM/external output, audible playback, non-ASCII endpoint names, quit/relaunch.
- [ ] Both manual ZIPs: extract/launch without a development Qt installation; open and save a MIDI file.
- [ ] Open Format 0/1, draw/move/resize/erase, copy/paste, undo/redo, save as a new file, close/reopen; compare notes, tempo and markers with another MIDI reader.
- [ ] Rehearsal markers within a measure, multiple same-tick markers, Unicode, marker text edit and color-only edit; save/reopen and whole-measure clipboard operations.
- [ ] Pedal CC64/66/69: mute, pause, stop and output changes; no persisting held notes after the queued-output tail.
- [ ] Large/high-PPQN files: Insert Measures and Scale Note Length near the supported limit; rejected edits retain data and undo history.
- [ ] Malformed tempo file: explained rejection, no crash or replacement of the active document/source file.
- [ ] Piano roll horizontal boundaries at several zoom/window sizes and display scales.

Minimum OS, clean-machine coverage, Intel runtime, external devices/hot unplug and Windows ARM remain incomplete unless explicitly recorded. Signing/notarization is unavailable. Do not run the disposable-CI installer smoke scripts on a personal computer.

## Candidate provenance and hashes

Fill from the completed draft workflow; do not infer the final tag commit from the pre-release fixes commit above. A defect discovered after tagging requires a new patch candidate, not moving a tag or overwriting a published release.
