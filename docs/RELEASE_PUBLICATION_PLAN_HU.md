# Speed MIDI Editor – fejlesztési és kiadási terv

Frissítve: **2026. október 12.** Aktuális, publikált kiadás: [0.1.5](https://github.com/RobCZart82/Speed-MIDI-Editor/releases/tag/v0.1.5). Ez a dokumentum a következő javító kiadás prioritásait és lezárási feltételeit is rögzíti; új verziószám vagy publikálási időpont még nincs kijelölve.

## Ellenőrzött kiinduló állapot

A felülvizsgált main commitja `d35866af35760f9cfaca386b312468877bfd4752`. A [Windows](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/38093326625), [macOS Universal/memóriaellenőrzés](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/38093326683) és [Linux backend](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/38093326651) ellenőrzése sikeres. Ez a dokumentációs frissítés nem új teljes kódaudit vagy új hardveres teszt.

A fenntartó a 0.1.5 draft tényleges Windows EXE és macOS PKG csomagját Windows 10 x64-en, illetve macOS Tahoe 26.7-en kipróbálta, működőnek találta, és kifejezetten engedélyezte a publikálást. Az egyes szerkesztési, lejátszási, frissítési lépéseket, MIDI-eszközöket és a Mac architektúráját nem részletezte; a két ZIP külön kézi próbája nincs igazolva.

## Javítások és ellenőrzések

A [kiadási megjegyzések](RELEASE_NOTES_0.1.5.md) rögzítik az egérműveletek túlcsordulás-védelmét, a pontos markermegjelenítést és navigációt, a páratlan PPQN ütésrácsát, valamint az útmutatók és buildrendszer javításait. Az [ellenőrzött kiadási workflow](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/38088169123) minden feladata sikeres. A részletes helyi, CI-, csomag- és fenntartói bizonyítékot a [QA-napló](RELEASE_QA_0.1.5.md) tartalmazza.

A tag változatlanul a `905c024f5bf5258c1772861751df9b5cf4f3f7f4` commitra mutat. A kiadás a már kipróbált négy csomagot és SHA256SUMS.txt fájlt tartalmazza. A későbbi dokumentáció és csomagellenőrző eszköz nem változtatja meg a binárisokat vagy a taget. A README a v0.1.5 letöltéseire mutat.

A PR-eket a főághoz igazítás és sikeres Actions ellenőrzések után kell beolvasztani; a beolvasztott főág ellenőrzéseit is meg kell várni. A kiadás végleges leírása a jelenlegi dokumentációra hivatkozik, mivel a taghez tartozó történeti dokumentumok még jelöltként írják le a verziót.

## Következő fejlesztések – prioritás és lezárás

A P0 az újonnan reprodukált összeomlás, adatvesztés vagy használhatatlan csomag azonnali javítása. A P1 az alábbi ellenőrzések és az ezekből igazolt hibák javítása. A P2 az ismert adatkezelési korlát vizsgálata. A sorrend nem jelenti azt, hogy a már publikált 0.1.5 visszamenőleg teszteletlennek minősül; a hiányzó részletes lefedettséget pótolja.

| Azonosító / prioritás | Feladat és jelenlegi állapot | Mikor kész? |
| --- | --- | --- |
| DEV-01 / P1 | Windows és macOS Manual Install ZIP külön kézi próbája fejlesztői Qt nélküli gépen. **Nincs rögzített kézi eredmény** a részletes QA-naplóban. | Mindkét rendszerre rögzített csomag/hash, OS/CPU, indítás, MIDI output/hallható lejátszás, mentés/újranyitás és kilépés/újraindítás eredménye. Hiányzó runtime vagy indítási hiba javítva, csomagregresszióval. |
| DEV-02 / P1 | Minimum OS, tiszta gép és Intel Mac lefedettség. **Hiányos**; Universal 2 bináris nem bizonyít Intel futtatást. | Windows 10 1809 és macOS 13 minimumállítások, Windows 11 x64, Intel és Apple Silicon tényleges futtatása külön dokumentálva; tiszta telepítés és előző publikált verzióról frissítés tesztelve. Elérhetetlen konfiguráció **NOT RUN**, vagy a támogatási állítás egyértelműen szűkítve. |
| DEV-03 / P1 | Valódi MIDI input/output, külső eszköz, nem ASCII eszköznév, lecsatlakoztatás és pedálállapotok. **Hiányos**; a Windows CI eszköztesztje eszköz nélkül kimaradhat. | Mapper/GS Wavetable, elérhető macOS GM és külső MIDI cél eredménye rögzítve. Ismételt nyitás/írás/zárás, outputváltás, aktív lejátszás közbeni lecsatlakoztatás, CC64/66/69 némítás/szünet/stop próbája: nincs összeomlás, beragadt állapot vagy tartós felületi lefagyás. Nem támogatott pedált az eszköz korlátjaként kell rögzíteni. |
| DEV-04 / P1 | Szerkesztés, fájlintegritás és piano roll részletes csomagszintű próba. Automatizált regressziók **PASS**, teljes kézi mátrix **nincs külön rögzítve**. | Format 0/1, rajzolás/move/resize/erase, vágólap, undo/redo, pontos/többszörös/Unicode marker, PPQN 481 és 4/8, nagy tick-határok és hibás tempó próbája rögzítve. Mentés/újranyitás másik MIDI-olvasóval összevetve; elutasított művelet után adat és undo változatlan; rácsvonalak több zoom/DPI mellett láthatók. |
| DEV-05 / P1 | Fordítás a minimum **Qt 6.8 / CMake 3.21** verziókkal. **Nincs rögzített minimumverziós eredmény**; a kiadási CI Qt 6.10.3-at használ. | Pontos Qt/CMake/compiler verzióval Windows és macOS configure/build/CTest eredmény rögzítve, a két alsó verzióhatár együtt ellenőrizve. Igazolt inkompatibilitás javítva regresszióval, vagy a minimumkövetelmény összehangolva a CMake, README és CONTRIBUTING fájlban. |
| DEV-06 / P1 | Statikus kódelemzés: elsőként egy reprodukálható cppcheck, clang-tidy vagy scan-build futás. **Nincs rögzített eredmény**, külön CI-lépés nincs beállítva. | Eszköz/verzió, konfiguráció, vizsgált alkalmazás- és módosított backendfájlok, jelentés rögzítve. A jelzések ellenőrizve; igazolt memória-, nullpointer-, túlcsordulási és élettartamhibák javítva célzott regresszióval. Kivétel csak indokolt, szűk körű lehet; az elemző jelzése önmagában nem igazolt bug. |
| DEV-07 / P2 | Conductor SysEx és track-end adatok globális ütemműveleteinek vizsgálata. **Ismert, dokumentált korlát**, nem újonnan igazolt regresszió. | Insert/delete/copy/paste esetekben előtte/utána esemény- és tick-összehasonlítás, elvárt viselkedés és lefedő teszt rögzítve. Csak igazolt helytelen adatkezelésből legyen javítás; SysEx playback/Thru új funkció nem része ennek a tervnek. |

Minden feladatnál **PASS / FAIL / NOT RUN / SKIP indokkal** szükséges, pontos commit- vagy csomagazonosítóval. Korábbi verzió próbája nem új bináris bizonyítéka. A CI-ben kihagyott eszközteszt nem hardveres PASS. Új adatvesztési vagy crash reprodukció felülírja a fenti sorrendet: reprodukáló teszt, kis javítás, releváns ellenőrzés, majd zöld PR és main.

## Következő javító kiadás – ellenőrzési sorrend

1. **Feladatzárás:** az érintett DEV-feladatok eredménye és reprodukciója legyen rögzítve; a kiadásba bevont igazolt hibák kapjanak regressziót. Nem tesztelhető környezet korlátja maradjon látható, ne kapjon automatikusan PASS-t.
2. **PR és main:** kis logikus commitok, felülvizsgálat, az aktuális PR-head minden releváns Actions ellenőrzése zöld; beolvasztás után a főági futások megvárása.
3. **Verzióelőkészítés:** a következő verziót külön ki kell jelölni; CMake, Windows erőforrás, generált About/macOS bundle, workflow alapértelmezés, CHANGELOG és új release notes/QA-napló egyezzen. A 0.1.5 publikált tage nem használható új main-commit kiadására.
4. **Pontos commitból draft:** Windows x64 EXE és Manual Install ZIP, macOS Universal 2 PKG és Manual Install ZIP, SHA256SUMS. DMG és Linux kiadási csomag nem cél. A változatlan tag csak sikeres platformellenőrzések után jöjjön létre.
5. **Csomagellenőrzés:** mind a négy hash, ZIP CRC, BUILD_INFO/verzió/commit/Qt, Windows GUI/x64/runtime, macOS Universal 2/signature/PKG payload és telepítőteszt legyen igazolt. A fájlintegritás-ellenőrzés nem publisher-hitelesítés vagy kézi QA.
6. **Új csomagok kézi próbája:** telepítés/frissítés, külön ZIP-indítás, hallható MIDI, mentés/újranyitás, érintett DEV-03/04 esetek; csomag/hash, OS/CPU, MIDI-eszköz és lépések szerinti eredmény az új verzió QA-naplójába kerüljön. A 0.1.5 általános fenntartói elfogadása nem vihető át automatikusan.
7. **Publikálási döntés:** blokkoló hiba lezárva; hiányzó lefedettség és ismert korlát pontosan leírva; a fenntartó a ténylegesen tesztelt draft publikálását engedélyezze. Tag utáni binárishibához új patch jelölt kell, meglévő tag/kiadott asset nem írható felül.
8. **Publikálás után:** README/linkek és státusz frissítése; mind az öt nyilvános asset hitelesítés nélküli elérhetőségének és a négy letöltés hash-egyezésének ellenőrzése, eredményrögzítés.

A 0.1.4 draft meghaladott és nem publikálható; [történeti QA-naplója](RELEASE_QA_0.1.4.md), tagje és csomageredete változatlan. A régebbi kiadások dokumentumai saját időpontjuk állapotát őrzik.

## Megmaradó korlátok

Minimum OS, Intel futtatás, tiszta gép és külső MIDI/hot unplug lefedettsége a DEV-01–04 feladatok része. Windows ARM nincs igazolt támogatásként kezelve; teljes Linux GUI/ALSA és kiadási csomag külön döntést igényel. Nincs publisher tanúsítvány vagy Apple notarizáció; ezek későbbi terjesztési döntések, nem igazoltan kész fejlesztések.

SysEx fájlmegőrzés van, playback/Thru továbbítás nincs; conductor SysEx és track-end adatok globális ütemműveleteknél nem mozognak/másolódnak a hangsávokkal (DEV-07). Sorba állított MIDI output röviden továbbfuthat; a pedálpróbánál ezt az ismert kimeneti késést is figyelembe kell venni. Teljes hibamentességet nem állítunk.
