# 0.1.5 candidate QA

Unpublished replacement for the superseded 0.1.4 draft. Record results for this candidate's exact commit and package hashes; earlier results are historical evidence only.

| Gate | Status | Evidence / next action |
| --- | --- | --- |
| Targeted audit fixes | PASS — local | Mouse interval validation, off-grid marker navigation/display, fractional-beat grid, guide corrections and retired qmake projects |
| Local macOS Debug ASan/UBSan tests | PASS | Complete application build; 12 CTest groups passed, localpeer_regression skipped because native IPC is unavailable in this sandbox. Strict ASan/UBSan halt-on-error; Qt 6.11.2, macOS 26.7 (25G229), AppleClang 21, arm64; isolated settings/offscreen editor, no hardware MIDI |
| Local macOS Release tests | PASS | Complete Release application build without sanitizers; 12 CTest groups passed, localpeer_regression skipped for the same native IPC restriction. All targeted CHECK assertions remain active in Release |
| PR Windows/macOS/Linux workflows | PASS | Head `2a9b76f4db43bf9e24858787cbdd0b91cb892f6c`: [Windows](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/37996602792), [macOS](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/37996602786), [Linux backend](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/37996603161) |
| Merged main checks | PASS | Commit `905c024f5bf5258c1772861751df9b5cf4f3f7f4`: [Windows](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/37999110866), [macOS](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/37999110869), [Linux backend](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/37999110862) |
| Exact-commit draft release build | IN PROGRESS | [Run 38088169123](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/38088169123), started 2026-10-10 from main at `905c024f5bf5258c1772861751df9b5cf4f3f7f4` with `v0.1.5` |
| Asset integrity/provenance | NOT RUN | Four packages, SHA256SUMS, ZIP CRCs, BUILD_INFO, executable/bundle/installer version and dependencies |
| Installer and package CI checks | PASS — main builds | Windows package/version and install/reinstall/uninstall passed; macOS Universal 2, signature, PKG payload/install/reinstall/receipt checks passed. Exact draft rerun is tracked separately above |
| Maintainer package/manual QA | NOT RUN | Exact EXE/PKG and both ZIPs; see checklist below |
| Publication | NOT RUN | Only the new, verified candidate can become public; README links remain on 0.1.3 |

## Hosted test evidence

- macOS main: 13 CTest groups passed in both strict ASan/UBSan Debug and Universal 2 Release, including native IPC. Qt 6.10.3 on macos-15.
- Windows main: 12 CTest groups passed; `winmm_device_smoke` was skipped because the runner has no MIDI device. Native IPC passed; Qt 6.10.3 on windows-2022.
- Linux main: timer and queue ASan/UBSan regressions and three release-tag Python tests passed. This is backend coverage, not a complete Linux GUI build.
- Seven release-asset checker tests pass locally, including damaged bytes, wrong source commit, incomplete/duplicate/unsafe manifests, unsafe archive paths and ZIP CRC damage. The checker does not establish publisher authenticity or replace platform/manual QA.

## Additional local evidence

- All three `tests/release_tag_regression.py` tests pass. Version 0.1.5 is consistent across CMake, Windows resources and the release-workflow default tag.
- The editor regressions execute rejected and valid mouse movement/drawing with undo/redo, five imported markers across local/whole-track/whole-measure navigation, exact marker positions/context, a rendered marker pixel check and PPQN 481 / 4/8 beat borders.
- The offscreen marker rendering was visually inspected. This is a source-built editor check, not a deployed installer or hardware playback trial.
- Retired qmake projects stop immediately with a clear CMake migration message rather than failing later on a missing QtWidgets header. CMake builds pass.

## Manual package checklist

- [ ] Windows EXE install/upgrade from 0.1.3, launch, output enumeration, audible playback, quit/relaunch and preferences.
- [ ] macOS PKG install/upgrade from 0.1.3, launch, output enumeration, audible playback, quit/relaunch and preferences.
- [ ] Both ZIPs: extract and launch without a development Qt installation; save and reopen a MIDI file.
- [ ] Import markers between cell/bar borders; Ctrl+PageUp/PageDown, multiple markers per cell/bar, text/color edits, save/reopen and undo/redo.
- [ ] Draw/move/resize/erase and large-note boundary rejection: unchanged data/undo after rejection; valid movement supports undo/redo.
- [ ] PPQN 481 in 4/8: header and piano-roll beat borders agree; position status uses the actual fractional beat length.
- [ ] Format 0/1 files, copy/paste, tempo/pedals, save as a new file and inspect it with another MIDI reader.

Record OS, architecture, MIDI destination, exact package name/hash and outcome. Minimum OS, clean machines, Intel playback, external MIDI/hot unplug and Windows ARM remain unverified unless recorded explicitly. Do not run disposable-runner installer smoke scripts on a personal computer.
