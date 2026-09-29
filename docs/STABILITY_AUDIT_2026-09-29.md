# Stability audit — 2026-09-29

Baseline: `main` at `d8425789d29c1dda057fec055be33740ffa6fe40`.
The original `upstream-1.1` snapshot is unchanged. No features are added.

## Findings and fixes

| Area | Evidence and correction |
| --- | --- |
| P0: WinMM x64 callbacks | The original backend crashes with `0xc0000005` while the native test opens Microsoft MIDI Mapper. Callback instance and MIDIHDR pointers were truncated to DWORD. All input/output callback declarations and definitions now use UINT and DWORD_PTR, including the conditional SysEx callback. |
| P0: failed output submission | With only callback signatures corrected, the original flush still crashes with `0xc0000005` under injected preparation/submission failures. Keep the header before clearing `m->hdr`; unprepare a failed submission without dereferencing NULL or falsely marking an in-use header free. |
| WinMM error cleanup | Failed output open cleared the descriptor before freeing it, leaking buffers and an event handle. Fix ordering, close stream handles with midiStreamClose, and clear the partially allocated buffer array to avoid double free. Input setup now unprepares an unqueued buffer, resets previously queued buffers on partial setup failure, and deletes its critical section. |
| MIDI lifecycle | Pm_Close frees the stream even on a backend error. Clear application stream state on both success and failure, before the worker can run again. Stop/join the worker before backend teardown; delete owned playback tracks. Preserve #5's atomic stop flag, QObject worker ownership, initialization checks and stale-device-ID fixes. Protect shared playback, thru and key-state reads, and the macOS sleep decision, with the existing recursive mutex. |
| SMF/parser | Parse into a temporary document so failed/repeated loads do not retain/append partial tracks. Bound clipboard metadata allocations to available data. Validate save preconditions and event ordering; keep file positions 64-bit and reject oversized track chunks. Generated track names use the existing one-byte SMF text conversion consistently. |
| UI correctness from #5 | Preserve input-availability reporting, nonzero note velocity, mouse-piano enum corrections, move-tool-only resizing, empty resize hit-test and unusable Fit All Tracks height guards. |
| Windows packaging | #5's Windows run built successfully but failed runtime verification: windeployqt copied vc_redist.x64.exe instead of DLLs. Copy the installed x64 MSVC CRT DLLs beside the executable, retaining the required-file checks. Both package workflows run CTest before packaging. |

## Regression coverage

- `winmm_regression`: real backend source with WinMM failure injection; high-address x64 callback instance/header pointers, preparation/submission/unprepare errors at latency 0 and 5, failed-open and every partial buffer allocation cleanup, handle counts and failed input-buffer submission. Checks remain active in Release builds.
- `winmm_device_smoke`: real Microsoft MIDI Mapper and GS Wavetable enumeration, open, silent controller message, callbacks and close; ten cycles at each of latency 0 and 5. No external hardware is selected. Returns CTest skip code 77 only when neither Microsoft output is enumerated; an enumerated device that fails is a failure.
- `midiinterface_regression`: injected close errors, cleared stream/key state, worker ownership and seek boundary cases.
- `smf_regression`: every truncation of a valid fixture, malformed VLQs, running status, chunk bounds, unsupported format/division, odd RIFF padding, roundtrip, atomic reload and malicious clipboard lengths.
- `smf_import_regression`: overlapping repeated notes paired FIFO, channel isolation and velocity-zero note-off.

Run all tests after a normal CMake build:

```sh
ctest --test-dir build -C Release --output-on-failure --no-tests=error
```

The Windows backend tests can also build without Qt:

```sh
cmake -S tests -B build-backend -A x64
cmake --build build-backend --config Release
ctest --test-dir build-backend -C Release --output-on-failure
```

Local validation uses Windows 11 x64 (build 26200), MSVC 19.44 and Qt 6.10.3.
The baseline native open and isolated flush tests both reproduce access violations;
the corrected backend passes, including both real Microsoft outputs (40 open/write/close cycles).
The complete application builds in Release. CI results are attached to the replacement PR.

## Pull request disposition

Checked all five existing PRs through GitHub metadata and local history; no standalone issues were returned by the repository issue search.

| PR | State at audit start | Disposition |
| --- | --- | --- |
| [#1](https://github.com/RobCZart82/Speed-MIDI-Editor/pull/1) — Prepare first cross-platform release | Merged; d3af7e6 | Preserve as release-preparation history. |
| [#2](https://github.com/RobCZart82/Speed-MIDI-Editor/pull/2) — Update README.md | Merged; b9f344b | Preserve; documentation change is already on main. |
| [#3](https://github.com/RobCZart82/Speed-MIDI-Editor/pull/3) — Avoid duplicate release workflow runs | Merged; 774e5f1 | Preserve; retain main-only push builds and PR builds. |
| [#4](https://github.com/RobCZart82/Speed-MIDI-Editor/pull/4) — Harden MIDI file parsing and note pairing | Merged; d842578 | Preserve; add executable regression coverage on top. |
| [#5](https://github.com/RobCZart82/Speed-MIDI-Editor/pull/5) — Fix Windows MIDI backend stability issues | Open, unmerged; 8f3e248 | Supersede with this tested branch based on current main. Its ten-file functional diff is retained or extended; no useful fixes are discarded. Close with a link to the replacement PR after it exists. Keep the original PR and branch for history. |

#5's [macOS run](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/36528450227) passed;
its [Windows run](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/36528450257) failed at CRT verification, not compilation.

## Remaining release checks

This audit is not proof that every legacy path is bug-free. Audible playback, interactive Preferences/save/relaunch testing with the packaged app, external MIDI input and hot unplug, clean-machine/minimum-OS tests, and macOS device runtime checks remain manual QA. CI without the Microsoft synth can only prove the deterministic backend tests. No release is published and no PR is merged by this audit.

The callback contract and close rules are documented by Microsoft in the [MIDI reference](https://learn.microsoft.com/en-us/windows/win32/multimedia/midi-reference) and [midiOutClose](https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/nf-mmeapi-midioutclose).
