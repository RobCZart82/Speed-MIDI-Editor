# Speed MIDI Editor 0.1.2

Draft candidate: a MIDI file integrity update. This release is not published yet.

## Changes since 0.1.1

- Refuse files with unsupported mid-measure meter changes, fractional-tick measure lengths, out-of-range meters or invalid key signatures. Explain the problem and preserve the original file and current document instead of silently losing data.
- Require exact payload lengths for MIDI time and key signatures; reject truncated and overlong malformed events.
- Preserve imported metronome-click and notation bytes through saves, measure editing, copy/paste and undo/redo. Existing clipboard formats remain supported.
- Run Linux backend sanitizer regressions as part of the exact-commit draft workflow, alongside Windows and macOS checks.

## Candidate downloads

- Windows x64: `Speed-MIDI-Editor-0.1.2-Windows-x64-Setup.exe` and `Speed-MIDI-Editor-Windows-x64.zip` for manual installation.
- macOS Universal 2: `Speed-MIDI-Editor-0.1.2-macOS-Universal.pkg` and `Speed-MIDI-Editor-macOS-Universal.zip` for manual installation. No DMG is produced.
- `SHA256SUMS.txt` covers these four downloads. Both packages include runtimes, English/Hungarian guides and license notices.

Windows installation is per-user. The macOS PKG installs into Applications and requires administrator approval. Neither platform has publisher signing configured; the Mac app is ad-hoc signed and not notarized. See the bundled README for installation instructions.

## Validation and publication gate

Regression coverage includes safe failed-open behavior, valid signature controls, 150 metronome/notation combinations through three save/reload cycles, clipboard region restoration, undo/redo and rejection of corrupted clipboard data. Automated workflows test platform builds, runtime deployment, installer payloads and install/reinstall behavior; Windows also tests uninstallation. The draft workflow rebuilds the exact source commit and runs both platform suites, macOS strict ASan/UBSan and Linux backend sanitizers before attaching downloads.

Interactive launch, audible MIDI output and save/reopen must be checked on the actual draft packages before publication. Earlier maintainer Windows GM and macOS playback evidence applies to preceding builds. Minimum-OS, clean-machine interactive launch, Intel Mac playback, Windows ARM emulation, external MIDI hardware and hot unplug remain incompletely covered. Targets remain Windows 10 1809+/Windows 11 x64 and macOS 13+ on Intel/Apple Silicon. Linux has backend tests but no release package.

## Known limits

- SysEx is preserved in files but is not transmitted during playback or MIDI Thru.
- Already queued MIDI output may continue briefly after Stop, Pause or Mute.
- Unsupported meter grids are safely refused rather than converted. Keep original MIDI files and backups.

Licensed under GPL-3.0-or-later; original-author and third-party notices accompany the packages.
