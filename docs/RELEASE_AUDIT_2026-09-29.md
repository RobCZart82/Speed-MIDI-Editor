# Follow-up release audit — 2026-09-29

Starting point: main `41596e2`, after PR #6 was merged. No open PRs remained at audit start. There were no published releases.

## Findings fixed

| Finding | Change / evidence |
|---|---|
| Windows release executable uses console subsystem | Set the GUI executable property through `WIN32`; package verifier inspects PE subsystem and x64 machine type. |
| Windows resource says Speedy Midi 1.1; Mac version metadata unset | Use Speed MIDI Editor 0.1.0 resource fields and CMake project version in the Mac bundle. |
| CMake advertises Qt 6.2 but uses QStyleHints::setColorScheme (Qt 6.8) | Require Qt 6.8 at configure time. CI remains on Qt 6.10.3. |
| Slow tempo in whole-note meter truncates to 0 BPM | New fixture fails before the fix; enforce positive integer BPM. Guard export against zero division and out-of-range 24-bit tempos. |
| Part extraction truncates old files before successful serialization | Stage each output with QSaveFile, check commit, and retain the previous file on failure. Earlier successfully extracted parts remain committed if a later part fails. |
| Corrupt clipboard counts can allocate unbounded objects after end of stream | Bound counts by available bytes and abort failed decoding before applying clipboard data. Regression covers negative/oversized/truncated metadata lists. |
| Package checks cover only part of macOS deployment | Check every Mach-O for both architectures and verify the ad-hoc signature, plugins, translations, docs and version. Windows checks GUI/x64 headers and required runtime files. |
| CI artifacts expire and no release workflow exists | Tag-driven reusable platform builds prepare a draft with both ZIPs and SHA256 checksums; reruns cannot replace assets of published releases. |
| README/guide facts are stale | Document current tests, manually confirmed playback, Qt requirement and current Apple first-launch path. |

## Release procedure

1. Merge the reviewed release-preparation PR only after both platform builds/tests/package checks pass.
2. Create `v0.1.0` at that tested commit on main. The tag must match the CMake version.
3. `Prepare release draft` runs both platform workflows on the same tag, then attaches both ZIPs and SHA256SUMS.txt to a draft release.
4. Review the draft notes and exact tagged package checks. Publish the draft when ready. No Apple notarization or Windows publisher signing is claimed.

## Recorded manual evidence

- Maintainer confirms the preceding Windows stability build plays through Microsoft GM without crashing.
- Maintainer confirms macOS launch, MIDI playback and save/reopen work. Exact OS version and Intel/Apple Silicon type are unspecified; this is not evidence for both architectures or minimum OS.
- New changes still require automated package verification on both OSes. Native hardware, clean-machine, minimum-OS and Windows ARM checks remain limited as stated in release notes.

The audit is targeted evidence, not a guarantee that all editor operations or all possible MIDI files are defect-free.
