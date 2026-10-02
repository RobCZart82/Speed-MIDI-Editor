# 0.1.3 release QA

Published on 2 October 2026 from immutable tag commit `613a97b20435bf3338d3dd8ffc281e8ea26c196b`. [Release](https://github.com/RobCZart82/Speed-MIDI-Editor/releases/tag/v0.1.3). The superseded 0.1.2 remains an unpublished draft.

| Check | Status | Evidence |
| --- | --- | --- |
| Local Release and strict ASan/UBSan regressions | PASS | All 13 groups, including separately executed local-peer tests; About 0.1.3 rendered |
| Preparation PR46 and merged-main platform checks | PASS | PR46 checked head 04bd68e; main 613a97b; runs 37045423271 Windows, 37045423310 macOS Universal/strict, 37045423322 Linux |
| Exact-commit release workflow | PASS | [37046563168](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/37046563168); all jobs successful |
| Downloaded package hashes and ZIP integrity | PASS | All four SHA256 values match SHA256SUMS; both ZIP CRC checks pass |
| Build provenance, runtimes and notices | PASS | BUILD_INFO 0.1.3 / 613a97b / Qt 6.10.3; Windows x64 GUI PE and VC runtimes; bundled licenses |
| macOS app and installer payload | PASS | Version 0.1.3, all Mach-O arm64+x86_64; expanded PKG byte/symlink manifest matches ZIP; deep strict codesign passes; no live-machine installer test was run by the agent |
| Actual Windows installer/application trial | PASS — maintainer report | On 2 October, maintainer reported testing installer and program, finding no visible or audible errors, and requested publication |
| Actual macOS installer/application trial | PASS — maintainer report | Same maintainer report for macOS |
| Individually documented save/reopen, 0.1.1 upgrade, OS/CPU/device, ZIP-only launch and extended manual matrix | NOT SEPARATELY RECORDED | General maintainer acceptance does not establish each individual step or configuration; no independent observation is claimed |
| Anonymous public downloads | PASS | All five public assets downloaded without authentication and matched the independently inspected draft bytes |

## Package SHA256

```text
60bcd2724ed253daa4061b0d103e6d19e0294d7e113237a7c07115776fcdb987  Speed-MIDI-Editor-Windows-x64.zip
72ca3d23df6e8013f429399b011e3434e52c6857d150c791187a09158513b27d  Speed-MIDI-Editor-macOS-Universal.zip
2961c4f25ea28df32a19df8dccc20e4d9083a7af5540628616178a5388e547fd  Speed-MIDI-Editor-0.1.3-Windows-x64-Setup.exe
9046ebb5a7d033f84edfd67e757b2ec352742945ccc471daec89de5298fbc0f3  Speed-MIDI-Editor-0.1.3-macOS-Universal.pkg
```

Minimum OS, Intel runtime, clean machines, Windows ARM, external devices and hot unplug coverage remains incomplete. No publisher certificate or Apple notarization is configured. SysEx file preservation does not provide playback/Thru transmission; queued output can continue briefly. The maintainer accepted publication after the actual dual-platform trial; unreported individual steps remain unreported.
