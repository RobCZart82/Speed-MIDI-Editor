# Speedy MIDI 1.1 technical audit

Audit basis: the supplied `speedymidi_src-1.1.zip` (2013 source release), Qt 6.11.2 macOS builds (Debug arm64 and Release Universal 2), inspection of the current GitHub Actions workflow, and the current source/docs. The untouched extracted archive is retained under [`../upstream-1.1/`](../upstream-1.1/). The user has manually tried interactive editing and Apple General MIDI playback during development. This is not a comprehensive regression test: automated tests, broad MIDI round-trip coverage, external-device testing, and clean-machine release packaging remain outstanding. The pre-release test matrix is recorded in [`RELEASE_READINESS.md`](RELEASE_READINESS.md).

## Project layout and dependencies

| Area | Findings | Porting consequence |
|---|---|---|
| Application | About 45 C++ implementation units, Qt Designer forms, a `.qrc` resource collection, translations, and application icons | Keep the existing UI and behavior; use CMake AUTOUIC/AUTOMOC/AUTORCC. The initial source remains together in `src/speedymidi` to avoid a disruptive structural rewrite. |
| Qt | Qt Widgets, XML, and Network are declared by `speedymidi.pro`; the GUI and editor are deeply coupled to Qt types | Require Qt 6 Widgets/Xml/Network. Migrate in place before considering component boundaries. |
| Build | `all.pro`, `speedymidi.pro`, and `portmidi.pro`; the application expects static PortMidi libraries in a relative `lib` folder | Top-level CMake now lists application sources/forms and builds PortMidi from the selected platform backend. |
| MIDI backend | Vendored PortMidi common code with CoreMIDI/CoreFoundation on macOS, WinMM on Windows, and ALSA on Linux | Keep the OS backend selection in CMake. The macOS implementation is historical and needs a current-SDK compile and device testing. |
| Single-instance support | Qt Solutions `QtSingleApplication`, `QtLocalPeer`, and `QtLockedFile` copied into the app | Old Qt platform macros and `QRegExp` require migration; verify licensing and consider replacing with a maintained implementation only if necessary. |
| Installer/examples | Inno Setup installer, installer artwork, and three sample MIDI files | Windows packaging material is not part of the first macOS build. Filenames in the source archive include accented characters. |

## Qt 4 to Qt 6 migration inventory

The following legacy APIs/patterns were found by source scan. This is the initial migration checklist, not an assertion that each occurrence is a compile error in every Qt 6 release.

| API/pattern | Locations | Planned replacement/action |
|---|---|---|
| `QApplication::UnicodeUTF8` passed to `QApplication::translate` | `speedymidiapp.cpp` (repeated translated action text) | Replaced with the Qt 6 three-argument overload; retain the source and catalog text. |
| `QString::toAscii()` | `smfdocument.cpp` | Replaced with `QChar::toLatin1()` to retain the old one-byte MIDI text conversion. Confirm behavior with MIDI text fixtures. |
| `QDesktopWidget` / `QApplication::desktop()` | `mainwindow.cpp` | Replaced with the current window's `QScreen::geometry()`; multi-display placement still needs runtime verification. |
| `QRegExp` | `qtsingleapplication/qtlocalpeer.cpp` | Replaced with `QRegularExpression`. |
| `qStableSort()` | `midiinterface.cpp`, `smfdocument.cpp`, `cs_playback.cpp`, `smfexporter.cpp` | Replaced with `std::stable_sort`, preserving the existing comparators and order. |
| `QMutex::Recursive` constructor enum | `midiinterface.cpp` | Replaced with `QRecursiveMutex`; locking and shutdown behavior still require runtime review. |
| Wheel-event code documents/uses legacy `delta()` semantics | `mousepianowidget.cpp`, `cs_utilities.cpp`, `cs_navigation.cpp` | Replaced with vertical `angleDelta()` values; trackpad behavior needs runtime verification. |
| Qt 4 module include style and old platform macros (`Q_WS_*`) | Especially Qt Solutions single-application files | Initial build blockers fixed, including header guards, `QMimeData`, and current include paths. Continue platform-conditional review; remove obsolete X11/Windows branches only with behavior understood. |
| Other removed/renamed APIs surfaced by compilation | `mousepianowidget.cpp`, `view.cpp`, `partextractiondialog.cpp` | Migrated wheel-event position, `QRegion::subtracted`, and `QHeaderView::setSectionResizeMode` usages. |
| String-based `SIGNAL`/`SLOT` connections | Throughout dialogs and controller code | Usually still supported but runtime-checked; convert selectively to typed connections as compile and behavior defects require. |

The Qt Designer `.ui` files can be retained. Build-system generation should expose any additional widget/API issues incrementally.

Several inherited C++/header files contained non-UTF-8 text bytes (mostly comments and translated/source strings). The working copy converts those files to UTF-8 so the modern Qt toolchain reads source text consistently; the Windows `.rc` resource script remains in its original encoding for resource-compiler compatibility.

## Platform-specific code

- **macOS:** `portmidi.pro` selects `pm_mac` CoreMIDI sources and CoreMIDI/CoreFoundation frameworks; the app project additionally links Carbon, CoreMIDI, CoreAudio, and CoreFoundation and hard-codes `CONFIG += x86`. `readbinaryplist.c` uses Carbon `FSRef`/`FSFindFolder` calls to locate MIDI preferences; validate these APIs and framework availability with a current Apple SDK and arm64 toolchain. PortTime has CoreFoundation and Mach clock alternatives. Remove the old architecture assumption and validate remaining SDK APIs and arm64 behavior. The historical bundle has no modern packaging/deployment configuration.
- **Windows:** `main.cpp` creates the `SpeedyMidiAntiUninstall` mutex, the app uses a Windows resource file, and PortMidi selects WinMM plus `winmm`. QtLockedFile has a Windows implementation. The qmake configuration has no ARM64-specific logic; Windows ARM64 must be checked separately.
- **Linux:** qmake selects ALSA (`PMALSA`, `-lasound`) and PortTime's Linux source. The initial CMake backend keeps this path as preliminary wiring, not as a release commitment.
- **Common:** application UI and SMF logic are mostly shared. OS MIDI implementation is concentrated in PortMidi. qmake's `unix:!macx` branch includes Linux-specific ALSA rather than a generic Unix backend.

## PortMidi API divergence and risks

This is not upstream PortMidi's current API. `third_party/portmidi/pm_common/portmidi.h` adds `PmInputCallbackProcPtr` and two callback arguments to `Pm_OpenInput`; `portmidi.c` stores them on the internal stream, and both the macOS CoreMIDI backend and Windows WinMM backend call the callback after receiving/queuing input. The app's `MidiInterface::openInput()` passes `midiInterfaceThread->inputCallbackProc` and its thread object. The Qt thread uses this path to wake/dispatch input handling rather than relying solely on periodic `Pm_Read` polling.

Therefore, dropping in upstream PortMidi would break compilation and may change input timing/dispatch. Before changing the backend, decide whether to (a) retain a documented callback adapter around upstream PortMidi, (b) move callback notification into a small platform layer, or (c) deliberately return to polling with a measured latency/CPU comparison. Review callback lifetime, locking, shutdown ordering, and error cleanup in `midiinterface.cpp` and `pmmacosxcm.c`; a matching signature alone does not establish thread safety.

The vendored PortMidi is old (2011-era backend, 2013 package) and its macOS backend must be audited against current CoreMIDI behavior. On macOS, MIDI transport is expected to use CoreMIDI; there is no need to introduce a separate audio engine.

## Licensing and provenance

- The application is labeled GPL version 3 or later; preserve the original GPL text and author notices. The repository `LICENSE` is copied from the supplied GPLv3 file.
- PortMidi carries a separate permissive license; its notice is retained at `third_party/portmidi/LICENSE.PortMidi`.
- The vendored Qt Solutions sources state alternative licensing paths including GPLv3, which is compatible with this GPLv3-or-later project when that option is used. Keep the complete original source headers and GPL license text. Review any Qt Solutions third-party notices if redistributing those components outside this GPL-compatible basis.
- `src/speedymidi/images/README.images` attributes assets to the original author, Qt, WebKit, Wikimedia, and Aha-Soft. The Aha-Soft terms allow inclusion within software but restrict redistribution of the icon itself; avoid distributing it as a standalone asset. Review whether the notice and license conditions are met for the fork.
- Qt is an external dependency, not vendored; release bundling needs a separate Qt license/deployment review.
- The source release identifies Speedy MIDI 1.1 and Holger Hoffmann. README attribution and the preserved original snapshot make the fork's origin explicit.

## Build and milestone status

The repository has a CMake target with explicit application/form lists and platform-selected PortMidi source sets. CMake builds the inherited translation catalogs to `.qm` files and places them beside the executable, matching the current loader path. A local Qt 6.11.2 macOS Release build has been configured for Universal 2 (`arm64;x86_64`). GitHub Actions is configured to deploy Qt into a macOS Universal 2 app bundle and create a downloadable ZIP, plus a Windows x64/MSVC 2022 package. Both first hosted runs stopped before CMake configuration while acquiring Qt: the action was given archive names as modules, and aqtinstall 3.3.0 also cannot resolve Qt 6.11's changed repository layout. The workflows now select `qtsvg`/`qttools` archives and pin Qt 6.10.3; reruns are pending. Neither workflow runs an automated test suite, and neither artifact has passed clean-machine testing. `ctest` currently reports zero registered tests. The editor now has blue-gray surfaces, flat project artwork, note drawing/erasing/moving/resizing tools, Fit All Tracks, and track-height controls; the user has manually confirmed core tool interaction and Apple General MIDI output. These manual checks do not replace repeatable regression tests.

A clean local Release rebuild succeeds, but reports warnings from legacy dependencies and code: deprecated Carbon folder APIs and a `MAXPATHLEN` redefinition in PortMidi, deprecated Qt event/checksum calls, and mismatched enum values in the mouse piano widget. These are not build failures, but the Mac-specific APIs deserve follow-up, especially for the supported macOS 13+ range. The previous compile-discovered `new[]`/`delete` mismatch in `smfdocument.cpp` was corrected. No official release exists yet. Before publishing a downloadable app, wait for both hosted package workflows to pass, test the archives on clean supported machines, and complete the licensing and manual QA checklist in `RELEASE_READINESS.md`.
