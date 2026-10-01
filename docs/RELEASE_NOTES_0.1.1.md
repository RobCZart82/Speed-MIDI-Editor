# Speed MIDI Editor 0.1.1

Stability update for the multi-track MIDI editor based on Speedy MIDI 1.1.

## Changes since 0.1.0

- Preserve exact MIDI tempo values and changes inside measures, including fractional BPM and precise playback timing.
- Preserve SysEx packets, track endpoints, unmatched notes, release velocities, source event order, bank/program selection and Unicode text through repeated saves.
- Keep reset-relative startup MIDI setup in order and make later track-property edits effective in export and playback.
- Preserve valid whole-measure boundaries at odd MIDI resolutions. Safely reject measure changes that would exceed the supported document length without changing the file or undo history.
- Release forwarded notes and sustain, sostenuto and Hold 2 pedals when MIDI Thru is disabled; improve recovery from input loss and MIDI output failures.
- Improve note editing, clipboard conversion/validation, undo/redo, part export names, seeking and queued playback cleanup.
- Add track-setup double-click and Cell length single-click hints, clearer About/license layout and missing F/E piano-roll separators.
- Replace macOS DMG packaging with a payload-verified PKG installer. Windows installers and both portable ZIP packages remain available.

## Downloads and installation

- **Windows x64:** run `Speed-MIDI-Editor-0.1.1-Windows-x64-Setup.exe`, or extract the entire Windows ZIP and start `SpeedMIDIEditor.exe`. The installer is per-user and includes an uninstaller and required runtimes.
- **macOS Universal 2:** run `Speed-MIDI-Editor-0.1.1-macOS-Universal.pkg`, or extract the macOS ZIP and copy `Speed MIDI Editor.app` to Applications. The installer requires administrator approval and installs guides/notices under `/Library/Application Support/Speed MIDI Editor`.
- `SHA256SUMS.txt` lists the SHA256 checksums of the four downloads. Qt and the required runtime libraries are included; English/Hungarian guides and license notices accompany the packages.

Packages have no publisher certificate. The Mac app is ad-hoc signed and is not notarized. Follow the included README's first-launch instructions if macOS Gatekeeper or Windows SmartScreen shows an unknown-publisher warning. The UI translations are English and German; the Hungarian guide does not imply a Hungarian UI.

## Validation and known limits

Release preparation runs the full applicable CTest suites on macOS and Windows, strict ASan/UBSan checks on macOS, and Linux backend sanitizer tests. Packaging checks verify architecture, bundled runtimes, notices and translations; installer tests check installation/reinstallation and Windows uninstallation. The release workflow rebuilds the exact tagged commit and attaches both installers, both ZIPs and their checksums.

Maintainer Windows 11 x64 Microsoft GM playback and macOS playback/save/reopen evidence comes from a preceding stability build. It does not establish new-candidate hardware coverage. Minimum-OS, clean-machine interactive launch, Intel Mac playback, Windows ARM emulation, external MIDI hardware and hot-unplug coverage remain incomplete. Build targets remain macOS 13+ (Intel/Apple Silicon) and Windows 10 1809+/Windows 11 x64; Linux is not a packaged release target.

- SysEx packets are preserved in files and supported by ordinary-track editing, but are not transmitted during playback or MIDI Thru. Apply device-specific SysEx setup separately.
- Already queued MIDI output may continue briefly after Stop, Pause or Mute; cleanup follows the bounded output lookahead.
- Arbitrary mid-measure meter changes and fractional-tick bar grids are not fully supported. Keep originals when editing files with unusual meter boundaries.
- Keep backups of important MIDI files. Automated coverage is not a guarantee for every device or file.

Licensed under GPL-3.0-or-later. The tagged source archive and original-author/third-party notices accompany this release.
