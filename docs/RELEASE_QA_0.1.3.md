# 0.1.3 candidate QA

This log applies only to the exact new candidate packages. Record package SHA256, BUILD_INFO commit, OS/CPU, MIDI device, steps and PASS/FAIL/NOT RUN. Do not reuse preceding-build interactive results. No 0.1.3 tag or packages have been created at this preparation step.

| Check | Status | Evidence |
| --- | --- | --- |
| Local macOS Qt 6.11.2 Release and strict ASan/UBSan Debug regressions | PASS | All 13 groups on the preparation working tree; local-peer tests separately outside the sandbox; rendered About version checked |
| Preparation PR and merged-main platform checks | NOT RUN | Await this preparation PR and main checks |
| Exact-commit release workflow | NOT RUN | Await candidate build |
| Downloaded assets, SHA256, ZIP integrity, BUILD_INFO, deployed runtimes and notices | NOT RUN | Await candidate assets |
| Windows candidate EXE and ZIP interactive launch; About 0.1.3 | NOT RUN | Actual package, preferably without developer Qt |
| Windows audible MIDI playback, Stop/Pause, quit/relaunch and saved port | NOT RUN | Record synth/device |
| Windows candidate editing, save/reopen and independent MIDI readback | NOT RUN | Use copies of originals |
| Windows 0.1.1 -> 0.1.3 upgrade, reinstall/uninstall and user-file preservation | NOT RUN | Separate test environment; same-version CI reinstall is insufficient |
| macOS candidate PKG and ZIP interactive launch; About 0.1.3 | NOT RUN | Record OS, Intel/Apple Silicon and first-launch behavior |
| macOS audible MIDI playback, Stop/Pause, quit/relaunch and saved port | NOT RUN | Record MIDI device |
| macOS candidate editing, save/reopen and Finder single-instance open | NOT RUN | Actual package |
| macOS 0.1.1 -> 0.1.3 upgrade and user-file/preference preservation | NOT RUN | Separate test environment |

Full candidate trials include Format 0/1, exact tempo and key changes, meter metadata, odd-PPQN whole-measure copy/paste, note edits, undo/redo, Solo/Mute, rejected invalid-file open without document loss, a long multi-track song, and piano-roll F/E boundaries at different zoom/scales.

Minimum OS, Intel runtime, clean machines, Windows ARM, external devices and hot unplug remain incomplete unless separately recorded. Publisher signing/notarization is unavailable. These limitations must be stated in release notes; no unrun test may be marked PASS.
