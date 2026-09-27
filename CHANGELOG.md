# Changelog

## 0.1.0 – Unreleased

- Preserve the original Speedy MIDI 1.1 source snapshot and identify it with the `upstream-1.1` tag.
- Add an initial Qt 6 and CMake build definition with platform-selected PortMidi source sets.
- Compile the existing Qt Linguist catalogs to `.qm` files and place them beside the executable, matching the inherited loader path.
- Replace the first removed Qt 4 APIs in the app/editor sources with Qt 6 equivalents (screen geometry, regular expressions, stable sorting, recursive mutex, text conversion, translation calls, and wheel deltas).
- Convert inherited non-UTF-8 C++/header text to UTF-8 while retaining the Windows resource script's original encoding.
- Record the migration, platform, MIDI callback, and licensing audit findings.
- Qt 6 application compilation and runtime verification remain outstanding.
