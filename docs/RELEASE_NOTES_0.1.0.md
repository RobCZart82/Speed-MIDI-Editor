# Speed MIDI Editor 0.1.0

First community release of the multi-track MIDI editor based on Speedy MIDI 1.1.

## Downloads

- **Windows installer:** run the `Windows-x64-Setup.exe` download. It installs for the current user, includes an uninstaller and Start menu shortcut, and does not require administrator rights.
- **macOS disk image:** open the `.dmg` and drag `Speed MIDI Editor.app` onto the Applications shortcut.
- ZIP packages remain available for portable/manual installation.

- **Windows x64:** extract the entire ZIP and start `SpeedMIDIEditor.exe`. Keep the DLLs and subfolders beside it. Windows 10 1809+ and Windows 11 are the build targets; Windows 11 x64 MIDI playback has been manually confirmed.
- **macOS Universal 2:** extract the ZIP and copy `Speed MIDI Editor.app` to Applications. The bundle contains both Apple Silicon and Intel code and targets macOS 13+. macOS playback and save/reopen have been manually confirmed; the test machine's exact OS and architecture were not recorded.
- `SHA256SUMS.txt` contains checksums of the release downloads. Qt and the required runtime libraries are included.

These packages have no publisher certificate. The Mac app is ad-hoc signed, not Apple-notarized. After trying to open it, use **System Settings → Privacy & Security → Open Anyway** if macOS blocks it, following [Apple's instructions](https://support.apple.com/en-am/guide/mac-help/open-a-mac-app-from-an-unknown-developer-mh40616/mac). Windows may also show an unknown-publisher warning.

## Reliability fixes

- Fix Windows x64 crashes when selecting Microsoft MIDI Mapper or GS Wavetable Synth, plus failed buffer submission and device-open cleanup.
- Improve MIDI worker shutdown, device errors and shared-state handling.
- Start the Windows editor without a console window; correct executable and Mac bundle version metadata.
- Harden MIDI/RMID parsing, repeated loading, overlapping-note import and clipboard decoding.
- Preserve exact MIDI tempo values and changes within a measure, including fractional BPM, with precise playback timing and compatible speed conversion. Reject tempo values that cannot be represented in the MIDI file format.
- Preserve existing files when a normal save or individual part export fails.
- Run regression tests and verify deployed application contents before packaging either platform.

English and Hungarian user guides and third-party notices are included. The application UI translations currently include English and German; the Hungarian guide does not imply a Hungarian UI.

## Validation and limits

Windows 11 x64 Microsoft GM playback and macOS playback/save/reopen were confirmed by the maintainer on the preceding stability build. Each new package runs automated parser/import/MIDI lifecycle tests; Windows also runs deterministic WinMM failure tests. Real MIDI device smoke coverage is skipped explicitly when a hosted runner has no usable native device.

Minimum-OS installations, Windows ARM emulation, external MIDI hardware/hot unplug and clean machines without development tools have not all been verified. Linux is not a release target. Keep backups when editing important MIDI files.

Licensed under GPL-3.0-or-later. The tagged source archive accompanies this release; original author and third-party notices are retained.
