# Speed MIDI Editor 0.1.4

Unpublished release candidate. Correctness, MIDI playback cleanup and platform stability fixes since 0.1.3. Existing published downloads and tags remain unchanged.

## Changes

- Check large measure/cell insertions and note-length scaling before changing the document or undo stack. Reject unsupported tick ranges with an explanation instead of overflowing or collapsing notes to one tick.
- Reject malformed or zero FF51 tempo values on SMF import rather than discarding/truncating them.
- Preserve exact conductor marker ticks, multiple marker packets at the same tick, their order and raw payload bytes through repeated saves and whole-measure copy/paste. Preserve Unicode, empty and non-UTF-8 marker data. Editing marker text replaces imported packets; changing only its color preserves them.
- Introduce V5 clipboard records for lossless marker data while retaining older payloads. V1–V4 clients cannot represent multiple/raw/off-bar markers fully; off-bar markers are omitted from those legacy payloads so other data remains readable. Paste and Scale to Selection continues to scale track events rather than conductor properties.
- Release sostenuto (CC66) and Hold2 (CC69), alongside sustain (CC64), on affected channels after queued output when muting or pausing playback. Controller reset clears tracked pedal states.
- Handle CoreMIDI Unicode device names with sufficient UTF-8 storage and checked conversion/allocation. Release owned strings/names; roll back device descriptors and default IDs after partial enumeration failure.
- Release the Windows single-instance mutex and clarify WinMM pointer casts. Retain the tested x64 callback ABI/flush fixes.
- Resolve annotated release tags to their commit and refuse API failures, invalid objects or a tag pointing to another commit.

## Candidate downloads

- Windows x64: `Speed-MIDI-Editor-0.1.4-Windows-x64-Setup.exe` and `Speed-MIDI-Editor-Windows-x64.zip` for manual installation.
- macOS Universal 2: `Speed-MIDI-Editor-0.1.4-macOS-Universal.pkg` and `Speed-MIDI-Editor-macOS-Universal.zip` for manual installation. No DMG.
- `SHA256SUMS.txt` covers all four downloads. Packages include runtimes, English/Hungarian guides and license notices.

Windows installation is per-user; the macOS PKG installs into Applications and requires administrator approval. No publisher certificates are configured. The macOS app is ad-hoc signed and not notarized; follow the bundled README installation instructions.

## Validation and limits

PR48 and its merged-main Windows/macOS/Linux workflows passed. Local Windows Release tests passed 13/13, including native WinMM device cycling. Tests cover edit rejection atomicity and undo/redo, malformed tempos, exact marker/file and clipboard roundtrips, pedal channel/threshold/ordering cases, and CoreMIDI Unicode/conversion/allocation/partial-registration cleanup. macOS native and strict sanitizer checks passed. These are evidence for the fixes, not a claim that the new release packages have already passed manual QA.

The exact candidate build, asset hashes/provenance and manual installer/application results are recorded separately in [RELEASE_QA_0.1.4.md](https://github.com/RobCZart82/Speed-MIDI-Editor/blob/main/docs/RELEASE_QA_0.1.4.md). Earlier 0.1.3 user acceptance is not transferred to this candidate.

Targets remain Windows 10 1809+/Windows 11 x64 and macOS 13+ on Intel/Apple Silicon. Minimum OS, clean machines, Intel playback, external MIDI and hot unplug coverage is incomplete. Windows ARM emulation is unverified; Linux has backend tests but no release package.

SysEx packets are preserved in files but not sent during playback/MIDI Thru. Already queued output can continue briefly after Stop/Pause/Mute. Unsupported meter grids and per-track signature layouts are safely refused. Minimum Qt/CMake builds and additional static analysis remain follow-up coverage, not completed validation.

Licensed under GPL-3.0-or-later; upstream and third-party notices accompany the packages.
