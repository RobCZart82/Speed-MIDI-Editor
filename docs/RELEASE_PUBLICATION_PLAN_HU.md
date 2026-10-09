# Speed MIDI Editor – terv a 0.1.4 publikálásáig

Frissítve: 2026. október 9. A nyilvános kiadás jelenleg [0.1.3](https://github.com/RobCZart82/Speed-MIDI-Editor/releases/tag/v0.1.3). A következő javító kiadás 0.1.4; publikálása előtt az új binárisok ellenőrzése szükséges. A korábbi Windows/macOS felhasználói próba a 0.1.3-ra vonatkozik.

## Kész fejlesztés és igazolt eredmények

- [PR48](https://github.com/RobCZart82/Speed-MIDI-Editor/pull/48) beolvadt a main ágba (`5ad5faf27280b51d36b1ef8714945dc0dd9b182c`). A main Windows-, macOS Universal/memóriaellenőrzési és Linux backend futásai sikeresek.
- A beszúrás/skálázás túlcsordulása, hibás tempóimport, markeradat-vesztés, pedálos némítás/szünet, CoreMIDI név/memória/hibakezelés, Windows mutex és annotált release tagek javítva. Részletek: [összevont audit](AUDIT_FIXES_2026-10-09_HU.md).
- A [PR49](https://github.com/RobCZart82/Speed-MIDI-Editor/pull/49) beolvadt a main ágba (`cea096bf9071d910f92d780dca43d0929aa06193`); az előkészítő PR és main összes platformellenőrzése sikeres. A Windows helyi Release 13/13 tesztje és 0.1.4 EXE-verziója ellenőrizve.
- Ebből a pontos commitból elkészült a [0.1.4 draft](https://github.com/RobCZart82/Speed-MIDI-Editor/releases/tag/untagged-4a697dc665ddd19b5463). A [release workflow](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/37914807958) mind a hat munkája sikeres, a letöltött négy csomag hash-, ZIP-, verzió-, forráscommit- és függőségellenőrzése megtörtént. A részletek és az ellenőrzőösszegek a [QA-naplóban](RELEASE_QA_0.1.4.md) szerepelnek.

**A publikálásig hátralévő fő feladat:** a 4–6. lépés új csomagos kézi tesztje és eredményrögzítése, majd a 7. lépés fenntartói publikálási engedélye. Az 1–3. lépés kész; a korábbi kiadás próbája nem helyettesíti ezeket az új teszteket.

## Sorrend és publikálási feltételek

1. **Előkészítő PR és main ellenőrzése.** Egyezzen a CMake, Windows fájlerőforrás, generált About/bundle verzió és workflow alapértelmezett tag. Minden aktuális PR-ellenőrzés legyen zöld, majd main beolvasztás és főági ellenőrzés.
2. **Pontos commitból draft építése.** A `Prepare release draft` workflow main-ról, `v0.1.4` bemenettel induljon. Készüljön Windows x64 EXE és Manual Install ZIP, macOS Universal 2 PKG és Manual Install ZIP, valamint SHA256SUMS. DMG és Linux csomag nem kiadási cél. A tag csak sikeres platformellenőrzések után jöjjön létre.
3. **Letöltött csomagok független ellenőrzése.** Mind a négy hash egyezzen, a két ZIP CRC-je legyen jó, a BUILD_INFO a tag végleges commitját és Qt-verzióját azonosítsa. Ellenőrizni kell a Windows GUI/x64 binárist, verziót és runtime-okat; macOS-en minden Mach-O Universal 2 legyen, a bundle verzió, PKG payload és ad-hoc aláírás ellenőrzése sikerüljön. A CI telepítőteszt külön bizonyíték a tényleges felhasználói gépes próbától.
4. **Új csomagok kézi tesztje mindkét rendszeren.** Telepítés/frissítés 0.1.3-ról, indítás, MIDI output és hallható lejátszás, quit/relaunch, mentés/újranyitás. A ZIP-eket külön is ki kell próbálni fejlesztői Qt nélküli környezetben. OS-, CPU- és MIDI-eszközadatot, pontos csomagot és eredményt rögzíteni kell a QA-naplóban.
5. **Célzott correctness próba.** Nagy felbontású/hosszú fájl beszúrása és hanghossz-skálázása; elutasítás után változatlan dokumentum/undo. Format 0/1 mentés/újranyitás másik MIDI-olvasóval is. Ütemen belüli és azonos időpontú markerek, szöveg/szín szerkesztés, vágólap/undo/redo. Pedálos némítás/szünet/stop és macOS nem ASCII eszköznevek. A piano roll vízszintes határai több zoom/DPI mellett legyenek láthatók.
6. **Hibák és lefedettségi korlátok lezárása.** Publikálást akadályozó hiba esetén javítás, regresszió, új zöld PR/main és új patch draft. Már létrejött taget nem mozgatunk; publikált assetet nem írunk felül. Nem tesztelt minimum OS, Intel Mac, tiszta gép, külső MIDI/hot unplug vagy Windows ARM eredményt nem állítunk sikeresnek.
7. **Publikálás.** A QA és csomaglista legyen végleges, a fenntartó engedélyezze a tesztelt draft publikálását. Ekkor frissüljenek a README letöltési linkjei és a kiadási státusz. Ezután az öt nyilvános fájl hitelesítés nélküli letöltését és hash-egyezését is ellenőrizni kell.

## Nyitott, külön kezelendő lefedettség

Minimum támogatott OS, Intel runtime, tiszta gépek, külső MIDI és hot unplug lefedettsége hiányos. Windows ARM nincs igazolt támogatásként kezelve. Minimum Qt 6.8/CMake 3.21 build és további statikus kódelemzés még nem futott. Ezeknél teszt vagy a támogatási állítások pontosítása szükséges; nem igazolt eredményt nem helyettesít a CI zöld státusza.

Nincs publisher tanúsítvány vagy Apple notarizáció; a jelenlegi csomagok ezt dokumentálják. SysEx fájlmegőrzés van, playback/Thru továbbítás nincs. Sorba állított MIDI output röviden továbbfuthat. Ezek ismert korlátok; nem új feature-feladatok ebben a javító kiadásban.

## Korábbi kiadások megőrzése

A 0.1.3 nyilvános, változatlan kiadási commitja `613a97b20435bf3338d3dd8ffc281e8ea26c196b`. Csomagellenőrzése és fenntartói elfogadása a [0.1.3 QA-naplóban](RELEASE_QA_0.1.3.md) szerepel. A 0.1.2 meghaladott jelölt, nem publikálható; a korábbi tageket és csomagokat nem módosítjuk. Korábbi hasznos PR-t, taget vagy kiadási bizonyítékot nem törlünk.
