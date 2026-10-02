# Speed MIDI Editor 0.1.3

Draft candidate: MIDI file integrity and clipboard fixes. Publication awaits testing of the actual candidate packages. This candidate supersedes the unpublished 0.1.2 draft; the older tag and assets remain unchanged.

## Changes since the published 0.1.1

- Refuse unsupported mid-measure meter changes, fractional-tick bar lengths, malformed signatures and ordinary Format 1 tracks with time/key signatures outside the conductor. Failed open explains the problem and preserves the active document and original file.
- Prevent fractional-tick meter edits before changing the document or undo history. Preflight exports so an unsupported meter grid cannot be saved as a file that the editor cannot reopen.
- Preserve metronome-click and notation bytes through saves, measure editing, whole-measure copy/paste and undo/redo.
- Preserve exact key-signature tick positions, including within a measure, through saving and whole-measure clipboard operations.
- Correct clipboard bar arithmetic at odd PPQN resolutions before and after resolution conversion. Refuse fractional-tick source or destination grids without modifying the document, selection or undo history.
- Use the standard SMF key-signature range of seven flats through seven sharps consistently in import, the properties dialog, model, clipboard and export. Nonstandard values outside −7…+7, including files written with such values by older builds, are refused with an explanation; originals remain unchanged.
- Derive the About version from the build version and include Linux backend sanitizer tests in the exact-commit draft workflow.

## Candidate downloads

- Windows x64: `Speed-MIDI-Editor-0.1.3-Windows-x64-Setup.exe` and `Speed-MIDI-Editor-Windows-x64.zip` for manual installation.
- macOS Universal 2: `Speed-MIDI-Editor-0.1.3-macOS-Universal.pkg` and `Speed-MIDI-Editor-macOS-Universal.zip` for manual installation. No DMG is produced.
- `SHA256SUMS.txt` covers these four downloads. Packages include runtimes, English/Hungarian guides and license notices.

Windows installation is per-user. The macOS PKG installs into Applications and requires administrator approval. No publisher certificates are configured; the Mac app is ad-hoc signed and not notarized. Follow the bundled README installation instructions.

Targets remain Windows 10 1809+/Windows 11 x64 and macOS 13+ on Intel/Apple Silicon. Minimum-OS, clean-machine launch, Intel playback, Windows ARM emulation, external MIDI hardware and hot unplug are incompletely covered. Linux has backend tests but no release package.

## Validation and publication gate

Regression tests cover actual meter-dialog rejection, unchanged document/state/undo after invalid operations, invalid export with no output, non-conductor signature rejection, 16 source/target PPQN pairs through 32767, exact mid-measure keys across repeated saves and clipboard/undo operations, standard key boundaries, and 150 metronome/notation combinations. Release and strict ASan/UBSan suites include the existing parser, editing, playback and backend regressions.

The release workflow rebuilds one exact commit and gates draft assets on Windows and macOS builds/tests/package deployment and installer checks, macOS strict sanitizers, and Linux backend sanitizers. Asset hashes, ZIP integrity, BUILD_INFO provenance and deployed runtimes must then be independently checked.

Actual candidate interactive launch, audible MIDI output, save/reopen and upgrade from 0.1.1 must be recorded before publication. Preceding-build maintainer trials do not establish these outcomes for the new packages. The candidate QA log is [RELEASE_QA_0.1.3.md](https://github.com/RobCZart82/Speed-MIDI-Editor/blob/main/docs/RELEASE_QA_0.1.3.md).

## Known limits

- SysEx is preserved in files but is not transmitted during playback or MIDI Thru.
- Already queued MIDI output may continue briefly after Stop, Pause or Mute.
- Unsupported meter grids and per-track signature layouts are safely refused rather than silently converted. Keep original files and backups.

Licensed under GPL-3.0-or-later; original-author and third-party notices accompany the packages.
