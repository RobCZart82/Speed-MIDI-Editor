# 0.1.4 candidate QA — superseded

**Do not publish this candidate.** The follow-up audit found mouse-move overflow and off-grid marker display/navigation defects in this code. The replacement candidate is [0.1.5](RELEASE_QA_0.1.5.md). This record and the immutable v0.1.4 tag retain their original provenance.

Unpublished candidate. Updated 9 October 2026. The previous 0.1.3 maintainer trial does not establish outcomes for these new binaries. Record PASS, FAIL or NOT RUN, with exact commit/run, OS, architecture and MIDI device.

| Gate | Status | Evidence / next action |
| --- | --- | --- |
| Correctness fixes PR48 and merged main | PASS | Main `5ad5faf27280b51d36b1ef8714945dc0dd9b182c`; Windows [37906265741](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/37906265741), macOS [37906265769](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/37906265769), Linux [37906265755](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/37906265755) |
| Local Windows 0.1.4 Release checks | PASS | All 13 CTest groups, including 40 native WinMM open/write/close cycles; EXE FileVersion/ProductVersion both 0.1.4 |
| Version/documentation preparation PR and merged-main checks | PASS | [PR49](https://github.com/RobCZart82/Speed-MIDI-Editor/pull/49), head `205965b9034b86683389e3189ba6072faa6f4279`; main `cea096bf9071d910f92d780dca43d0929aa06193`; Windows [37912060765](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/37912060765), macOS Universal/strict [37912060798](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/37912060798), Linux [37912060754](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/37912060754) all successful |
| Exact-commit draft release build | PASS | [37914807958](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/37914807958), all six jobs successful; immutable tag `v0.1.4` points to `cea096bf9071d910f92d780dca43d0929aa06193`; release ID 407788543 remains draft |
| Download hashes and ZIP integrity | PASS | Downloaded all five assets; checked sizes/GitHub SHA256 digests, all four unique SHA256SUMS entries and both ZIP CRCs |
| Package provenance, version and dependencies | PASS | Both BUILD_INFO files identify 0.1.4 / `cea096bf9071d910f92d780dca43d0929aa06193` / Qt 6.10.3; downloaded Windows EXE has x64 GUI PE, FileVersion/ProductVersion 0.1.4 and required Qt/VC runtimes; macOS bundle version 0.1.4 and all 19 Mach-O files contain x86_64+arm64; PKG identity/version and three archived checksums pass |
| Automated installer and native macOS package checks | PASS — CI | Exact release run: Windows install/reinstall/uninstall; macOS PKG install/reinstall, expanded payload byte/symlink comparison and deep strict ad-hoc codesign verification. Native macOS validation ran in CI, not on the agent's Windows computer |
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

[Unpublished draft](https://github.com/RobCZart82/Speed-MIDI-Editor/releases/tag/untagged-4a697dc665ddd19b5463), created 9 October 2026. The four downloads and SHA256SUMS are independently verified. Subsequent documentation-only commits do not change this binary provenance or move the tag. Public downloads remain at 0.1.3 until approval.

```text
bf2d080e6b1e8f9ab80359c2fee9ad259702132896392dee198a717c16428bfa  Speed-MIDI-Editor-Windows-x64.zip
5d7276040de2a8b881c759e1b4bfe19919b0f386f3ab37daa9d8fdd547e85720  Speed-MIDI-Editor-macOS-Universal.zip
ec15e56d1d2665fc13df130a48e3119395fa252b3503f1647d9cfa060947c1d9  Speed-MIDI-Editor-0.1.4-Windows-x64-Setup.exe
882d22e7498497ca30273ff7b0c2c431ecadd6aa55b6eca5b3578a0bc8febf65  Speed-MIDI-Editor-0.1.4-macOS-Universal.pkg
```

The SHA256SUMS file itself has SHA256 `1c389e2cba989764deaf06a6da5889dc68e9a64fd9753536a7d273d1b6ed3bfa`. No independent live-machine installation or audible playback was performed by the agent. A defect discovered after tagging requires a new patch candidate, not moving a tag or overwriting a published release.

