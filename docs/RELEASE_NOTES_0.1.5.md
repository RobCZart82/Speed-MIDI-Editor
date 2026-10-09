# Speed MIDI Editor 0.1.5

Unpublished patch candidate replacing the superseded 0.1.4 draft. Public downloads remain at 0.1.3 until the new packages have passed their own checks. The v0.1.4 tag and assets keep their original provenance.

## Fixes

- Validate mouse draw/resize/move note intervals before changing the live document. Large/high-PPQN notes cannot acquire an overflowing or out-of-range end tick; rejected movement leaves data and undo history intact.
- Keep marker navigation selections and anchors on valid cell borders while tracking the exact marker position separately. Repeated Ctrl+PageUp/PageDown progresses through several markers inside one cell instead of producing an invalid editor state.
- Display imported markers at their exact positions inside a measure, including several markers in one measure, and retain the correct marker context in following measures. File timestamps and raw marker packets remain unchanged.
- Calculate each beat border from the full tick fraction. Odd PPQN and fractional beat lengths no longer accumulate rounding errors in header/grid lines or the beat-position status calculation.
- Update the English/Hungarian guides to describe published downloads and distinguish PKG installation from app first launch. Explicitly retire legacy qmake projects in favor of the supported top-level CMake build.

All fixes from the [0.1.4 candidate](RELEASE_NOTES_0.1.4.md) are included: edit rejection atomicity, exact/raw marker preservation, tempo validation, pedal cleanup, CoreMIDI names/memory handling and immutable release-tag checks.

## Candidate downloads

- Windows x64: `Speed-MIDI-Editor-0.1.5-Windows-x64-Setup.exe` and `Speed-MIDI-Editor-Windows-x64.zip`.
- macOS Universal 2: `Speed-MIDI-Editor-0.1.5-macOS-Universal.pkg` and `Speed-MIDI-Editor-macOS-Universal.zip`.
- `SHA256SUMS.txt` covers all four packages. Guides, required runtimes and license notices are bundled. No DMG or Linux release package is produced.

Windows installs per user. The macOS PKG installs into Applications with administrator approval. No publisher certificates are configured; the macOS app is ad-hoc signed and not notarized. Follow the bundled README/guide for first-launch approval.

## Validation and limits

The targeted regression cases use actual mouse/controller events and a format-1 MIDI import with five off-grid markers. Checks also cover valid large-note movement with undo/redo and a PPQN 481, 4/8 grid. Exact local/CI results and the new packages' provenance belong in [RELEASE_QA_0.1.5.md](RELEASE_QA_0.1.5.md); earlier candidate checks are not transferred to this build.

Targets remain Windows 10 1809+/Windows 11 x64 and macOS 13+ on Intel/Apple Silicon. Minimum OS, clean machines, Intel runtime, physical MIDI input/output, external devices/hot unplug and Windows ARM emulation have incomplete coverage. Automated tests cannot certify every MIDI file or device.

Conductor SysEx packets remain global file data: ordinary track editing is supported, but global measure insertion/deletion/copy does not shift/copy conductor SysEx or its track-end metadata. SysEx is preserved in files but not sent during playback/MIDI Thru. Already queued output can continue briefly after Stop/Pause/Mute. Unsupported meter layouts are rejected.

Licensed under GPL-3.0-or-later; original-author and third-party notices remain in the packages.
