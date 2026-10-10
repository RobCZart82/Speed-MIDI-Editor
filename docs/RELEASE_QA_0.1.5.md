# 0.1.5 candidate QA

Unpublished replacement for the superseded 0.1.4 draft. Record results for this candidate's exact commit and package hashes; earlier results are historical evidence only.

| Gate | Status | Evidence / next action |
| --- | --- | --- |
| Targeted audit fixes | PASS — local | Mouse interval validation, off-grid marker navigation/display, fractional-beat grid, guide corrections and retired qmake projects |
| Local macOS Debug ASan/UBSan tests | PASS | Complete application build; 12 CTest groups passed, localpeer_regression skipped because native IPC is unavailable in this sandbox. Strict ASan/UBSan halt-on-error; Qt 6.11.2, macOS 26.7 (25G229), AppleClang 21, arm64; isolated settings/offscreen editor, no hardware MIDI |
| Local macOS Release tests | PASS | Complete Release application build without sanitizers; 12 CTest groups passed, localpeer_regression skipped for the same native IPC restriction. All targeted CHECK assertions remain active in Release |
| PR Windows/macOS/Linux workflows | PASS | Head `2a9b76f4db43bf9e24858787cbdd0b91cb892f6c`: [Windows](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/37996602792), [macOS](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/37996602786), [Linux backend](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/37996603161) |
| Merged main checks | PASS | Commit `905c024f5bf5258c1772861751df9b5cf4f3f7f4`: [Windows](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/37999110866), [macOS](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/37999110869), [Linux backend](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/37999110862) |
| Exact-commit draft release build | PASS | [Run 38088169123](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/38088169123), completed successfully 2026-10-10 from main at `905c024f5bf5258c1772861751df9b5cf4f3f7f4` with `v0.1.5` |
| Asset integrity/provenance | PASS | All four downloaded draft-build assets match GitHub release SHA256 digests and the downloaded SHA256SUMS.txt. Both ZIP CRCs and BUILD_INFO records match 0.1.5 / `905c024f5bf5258c1772861751df9b5cf4f3f7f4` / Qt 6.10.3. Windows PE x64 GUI/runtimes and macOS Universal 2/signature/PKG payload match verified independently |
| Installer and package CI checks | PASS — main and exact draft | Windows package/version and install/reinstall/uninstall passed; macOS Universal 2, signature, PKG payload/install/reinstall/receipt checks passed. The exact draft rerun also passed all platform installer/package checks |
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

## Exact draft asset hashes — 2026-10-10

Source: `905c024f5bf5258c1772861751df9b5cf4f3f7f4`, immutable tag `v0.1.5`. [Draft release](https://github.com/RobCZart82/Speed-MIDI-Editor/releases/tag/untagged-ed74710a6e2bbb3e6ba2). Downloaded workflow artifacts match the release asset digests; the downloaded release manifest also matches all four files.

| Asset | SHA256 |
| --- | --- |
| SHA256SUMS.txt | `47996d195fac61548f0735417f4d8f2c81dff2c17dd7bf6929e2bbd668635b28` |
| Speed-MIDI-Editor-0.1.5-macOS-Universal.pkg | `dfedd724483a7c4f85b31787381f9399cd4ae01144ca15ee311b2bbf7f31ed78` |
| Speed-MIDI-Editor-0.1.5-Windows-x64-Setup.exe | `b81bd899281426b4b80b7a042c6990413d24bae7d2ddb084ed3ccef16c30add2` |
| Speed-MIDI-Editor-macOS-Universal.zip | `3f693e0d6a4f993c96d5fe3a53e089cb7f20b1cf5c46334b0e1d82d2acb9bae1` |
| Speed-MIDI-Editor-Windows-x64.zip | `34312c07781f25821e08c21f8fcb2a9bbf6ff90cd48f10dcc9a158f7d7341af3` |

No installer was run on the maintainer's computer, and no audible or hardware MIDI QA is claimed. The seven checker regressions and workflow gate are follow-up tooling in PR #52, not changes to the tagged application binaries.
