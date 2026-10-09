# Az összevont audit javításai – 2026-10-09

Kiinduló `main`: `e53c932c4c034a9f18d43468610342c2260506c4`.
A változtatások célja az igazolt hibák javítása; új kiadás publikálása nem része ennek a csomagnak.

## Javítások logikus sorrendben

| Prioritás | Igazolt probléma | Javítás és regresszió |
| --- | --- | --- |
| P1 | Ütembeszúrás egészszám-túlcsordulással, sérült visszavonással | 64 bites előzetes tartományellenőrzés minden érintett sávra és karmestersáv-eseményre; elutasítás az undo művelet megnyitása előtt. Határértékek, több esemény és mentés/undo/redo tesztelve. |
| P1 | Hanghossz-skálázás túlcsordulása egy tickre rövidíthette a hangot | Az összes kiválasztott hang előzetes ellenőrzése; érvénytelen eredménynél a teljes művelet módosítás nélkül elutasítva. |
| P2 | Némítás/szünet után a sostenuto vagy Hold2 pedál aktív maradhatott | CC64, CC66 és CC69 állapotkövetés; kikapcsolás az érintett csatornákon a kiküldött események után. CC121 törli a követett állapotot. Küszöbérték-, csatorna- és sorrendtesztek. |
| P2 | Hibás hosszúságú vagy nulla FF51 tempóadat csendben elveszett | Pontosan három bájt és pozitív tempó szükséges; hibás fájl elutasítva egyértelmű importhibával. Format 0/1 tesztek. |
| P2 | Karmestersáv-markerek kerekített időpontja, azonos időpontú markerek elvesztése | Eredeti tick, bájtok és sorrend megőrzése; ismételt mentés/újranyitás, Unicode, üres és nem UTF-8 adat tesztelve. V5 vágólapadat és valódi másolás/beillesztés/undo/redo regresszió. |
| P2 | Annotált release tag objektumazonosítója commitként lett összehasonlítva | Annotált tagek feloldása commitra; csak valódi 404 esetén új tag létrehozása. API-hibák, eltérő commit, ciklus és nem commit objektum elutasítva. Hálózat nélküli Python-tesztek. |
| P2/P3 | CoreMIDI névkonverzió túl kicsi pufferrel, ellenőrizetlen hibák és névmemória-szivárgás | UTF-8 maximális pufferméret, konverzió/allokáció ellenőrzése, tartaléknév, CoreFoundation- és eszköznév-tulajdonlás rendezése. Natív macOS-tesztek bővítve. |
| P3 | Windows példánymutex handle nem volt explicit felszabadítva | Élettartamhoz kötött felszabadítás; sikertelen létrehozás naplózása. WinMM eszközazonosító-castok pointerméretének pontosítása. |

A korábbi WinMM x64 callback ABI és flush javításokat megőrizzük; a natív output regresszió újra lefutott.
A Copilot általános állításai közül csak a jelenlegi forrásban igazolható problémák kerültek javításra.

## Ellenőrzési állapot

- **PASS:** Windows 11 x64, MSVC 2022, Qt 6.10.3, Release fordítás.
- **PASS:** teljes helyi CTest: 13/13, köztük editor, SMF/import, vágólap/undo, playback és WinMM.
- **PASS:** natív Windows output smoke: Microsoft MIDI Mapper és GS Wavetable Synth, összesen 40 open/write/close ciklus, 0 és 5 ms latency. Ez nem hallható lejátszási vagy időzítési mérés.
- **PASS:** release tag Python-regressziók; a tesztek nem hoznak létre valódi taget vagy release-t.
- **PASS:** PR48 és a beolvadt main Windows-, macOS Universal/natív CoreMIDI/sanitizer és Linux backend ellenőrzése. A következő 0.1.4 csomagok külön eredménye a [kiadási QA-naplóba](RELEASE_QA_0.1.4.md) kerül.
- **NOT RUN:** új csomag kézi telepítése és hardveres lejátszási teszt macOS-en; új Windows telepítő kézi tesztje; minimum Qt/CMake verziós build.

## Kompatibilitási határok

Az új V5 vágólap megőrzi a többes és nyers markereket. A V1–V4 payload továbbra is elérhető régebbi programok számára; ezek nem tudják teljesen ábrázolni az új markeradatot. Ütemhatáron kívüli markerek a régi payloadból kimaradnak, hogy a régi olvasók elfogadják a többi adatot. A „Paste and Scale to Selection” továbbra is sáv-eseményeket skáláz; karmestersáv-adatok átvitele ebben a műveletben nem új funkció.

## Következő kiadás előtti lépések

1. A PR minden releváns Actions ellenőrzése legyen zöld az aktuális commiton; csak ezután main beolvasztás.
2. A main végleges commitjáról készülő draft csomagokat külön tesztelni kell: Windows EXE + Manual Install ZIP, macOS PKG + Manual Install ZIP. DMG nem kiadási cél.
3. Mindkét rendszeren telepítés, indítás, MIDI output, pedálos némítás/szünet/stop, szerkesztés és mentés/újranyitás; a piano roll rács kézi vizuális ellenőrzése több zoom/DPI mellett.
4. Nagy felbontású/hosszú fájlokkal beszúrás és hanghossz-skálázás; elutasítás után dokumentum és undo történet változatlansága.
5. Marker/tempó tesztfájlok ellenőrzése másik MIDI-programban; nem ASCII CoreMIDI eszköznevek és ismételt indítás macOS-en.
6. Új verzió release notes, csomaglista, ellenőrzőösszegek és [publikálási terv](RELEASE_PUBLICATION_PLAN_HU.md) frissítése. Meglévő publikált tag és asset nem írható felül.
7. A draft csak dokumentált kézi QA és publikálási jóváhagyás után váljon nyilvánossá.
