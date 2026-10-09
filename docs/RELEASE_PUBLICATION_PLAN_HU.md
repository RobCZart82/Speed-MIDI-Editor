# Speed MIDI Editor – terv a 0.1.5 publikálásáig

Frissítve: 2026. október 9. A nyilvános kiadás jelenleg [0.1.3](https://github.com/RobCZart82/Speed-MIDI-Editor/releases/tag/v0.1.3). A következő javított jelölt **0.1.5**. A 0.1.4 draft meghaladott: az új audit hangmozgatási túlcsordulást és a pontos markeridők megjelenítési/navigációs hibáit találta. A régi tag változatlan marad; a draft törlése nem előfeltétele az új kiadásnak.

## Javítás és tesztelés

A 0.1.5 a [korábbi javításokra](AUDIT_FIXES_2026-10-09_HU.md) épít, és az új audit hat megállapítását kezeli. Részletek: [0.1.5 kiadási megjegyzések](RELEASE_NOTES_0.1.5.md), [0.1.5 QA-napló](RELEASE_QA_0.1.5.md).

- Teljes hangintervallum-ellenőrzés az egérrel rajzolás/átméretezés/mozgatás előtt; a túl nagy mozgatás változatlanul hagyja az adatot és az undo-előzményt.
- Rácsra illesztett marker-kijelölés, külön pontos navigációs pozíció; több marker egy cellán belül is bejárható.
- Ütemen belüli markerek megjelenítése pontos időpontnál, következő ütemekben helyes markerelőzmény.
- Páratlan PPQN esetén pontos ütésvonalak és pozíciószámítás.
- Kiadott verziókhoz igazított HU/EN útmutató, külön PKG/app telepítési lépések; régi qmake projektek egyértelmű lezárása.

## A kiadás menete

1. **Javítások és regressziók.** Helyi Debug ASan/UBSan és Release tesztek; mindkét buildben a valódi import-, egér- és billentyűműveletek futnak. A kihagyott hardver/IPC tesztet külön kell jelölni.
2. **PR és main ellenőrzése.** Minden aktuális Windows/macOS/Linux ellenőrzés legyen sikeres. Beolvasztás után az új main commitot is ellenőrizni kell. A CMake, Windows erőforrás, generált About/bundle verzió és workflow tag egyezzen.
3. **Új draft pontos commitból.** A `Prepare release draft` workflow main-ról, `v0.1.5` bemenettel induljon. Készüljön Windows x64 EXE + ZIP, macOS Universal 2 PKG + ZIP és SHA256SUMS. A v0.1.5 tag csak az összes build/teszt után jöjjön létre; már létrehozott taget nem mozgatunk.
4. **Csomagellenőrzés.** Ellenőrzőösszegek, ZIP CRC-k, BUILD_INFO commit/Qt-verzió, Windows GUI/x64/fájlverzió/runtime és macOS Universal 2/bundle/PKG/aláírás. A CI telepítőpróbája külön bizonyíték a személyes gépes próbától.
5. **Az új csomagok kézi próbája.** EXE/PKG telepítés/frissítés, indítás, MIDI-kimenet és hallható lejátszás, mentés/újranyitás, quit/relaunch, valamint mindkét ZIP. A marker- és nagy tickértékes hibákra célzott próbák is kellenek. OS-, CPU-, MIDI-eszköz-, csomag- és eredményadat kerüljön a QA-naplóba. A 0.1.3 vagy 0.1.4 próbája nem igazolja az új binárist.
6. **Publikálás a sikeres próbák után.** A végleges, ellenőrzött draft válhat nyilvánossá. Ekkor frissüljenek a README letöltési linkjei; az öt nyilvános fájl letöltését és hash-egyezését újra ellenőrizni kell. Új hiba esetén javítás, regresszió és új patchjelölt szükséges.

## Megőrzött korábbi bizonyítékok

A [0.1.4 QA-napló](RELEASE_QA_0.1.4.md) őrzi a draft eredeti commitját, csomaghash-eit és CI-eredményeit; ezek nem igazolják a 0.1.5 javításait. A 0.1.4 draft nem publikálható. A 0.1.3 nyilvános és változatlan; [QA-naplója](RELEASE_QA_0.1.3.md) külön történeti bizonyíték.

## Lefedettségi korlátok

Minimum támogatott OS, Intel futtatás, fejlesztői Qt nélküli tiszta gép, külső MIDI/hot unplug és Windows ARM nincs teljesen ellenőrizve. Minimum Qt 6.8/CMake 3.21 build és további statikus elemzés külön követési feladat. Nem tesztelt eredményt nem helyettesít a CI zöld státusza, és teljes hibamentességet nem állítunk.

Nincs publisher tanúsítvány vagy Apple notarizáció. SysEx fájlmegőrzés van, playback/Thru továbbítás nincs; conductor SysEx és track-end adatok globális ütemműveleteknél nem mozognak/másolódnak a hangsávokkal. Sorba állított MIDI output röviden továbbfuthat. Ezek dokumentált korlátok, nem automatikusan lezárt hardveres tesztek.
