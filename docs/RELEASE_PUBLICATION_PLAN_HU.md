# Speed MIDI Editor – a 0.1.5 kiadási jegyzőkönyve

Frissítve: 2026. október 11. Aktuális kiadás: [0.1.5](https://github.com/RobCZart82/Speed-MIDI-Editor/releases/tag/v0.1.5). A fenntartó a draft tényleges Windows EXE és macOS PKG csomagját Windows 10 x64-en, illetve macOS Tahoe 26.7-en kipróbálta, működőnek találta, és kifejezetten engedélyezte a publikálást.

## Javítások és ellenőrzések

A [kiadási megjegyzések](RELEASE_NOTES_0.1.5.md) rögzítik az egérműveletek túlcsordulás-védelmét, a pontos markermegjelenítést és navigációt, a páratlan PPQN ütésrácsát, valamint az útmutatók és buildrendszer javításait. Az [ellenőrzött kiadási workflow](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/38088169123) minden feladata sikeres. A részletes helyi, CI-, csomag- és fenntartói bizonyítékot a [QA-napló](RELEASE_QA_0.1.5.md) tartalmazza.

A tag változatlanul a `905c024f5bf5258c1772861751df9b5cf4f3f7f4` commitra mutat. A publikálás a már kipróbált négy csomagot és SHA256SUMS.txt fájlt használja. A későbbi dokumentáció és csomagellenőrző eszköz nem változtatja meg a binárisokat vagy a taget. A README a v0.1.5 letöltéseire mutat.

A PR-eket a főághoz igazítás és sikeres Actions ellenőrzések után kell beolvasztani; a beolvasztott főág ellenőrzéseit is meg kell várni. A kiadás végleges leírása a jelenlegi dokumentációra hivatkozik, mivel a taghez tartozó történeti dokumentumok még jelöltként írják le a verziót.

## Következő kiadások

1. Hibajavítás és célzott regresszió, majd sikeres PR- és main-ellenőrzés.
2. Új verzió és új, változatlan tag; draft építése pontos commitból.
3. Hash-, ZIP-, BUILD_INFO-, platform- és telepítőellenőrzés.
4. A tényleges új csomagok kézi próbája, eredmények és hiányzó lefedettség rögzítése.
5. Fenntartói jóváhagyás után publikálás és aktuális letöltési dokumentáció.

A 0.1.4 draft meghaladott és nem publikálható; [történeti QA-naplója](RELEASE_QA_0.1.4.md), tagje és csomageredete változatlan. A régebbi kiadások dokumentumai saját időpontjuk állapotát őrzik.

## Megmaradó korlátok

Minimum OS, Intel futtatás, tiszta gép, külső MIDI/hot unplug és Windows ARM nincs teljesen ellenőrizve. A két ZIP kézi próbáját és az egyedi tesztlépéseket a fenntartó nem részletezte. Nincs publisher tanúsítvány vagy Apple notarizáció. SysEx fájlmegőrzés van, playback/Thru továbbítás nincs; conductor SysEx és track-end adatok globális ütemműveleteknél nem mozognak/másolódnak a hangsávokkal. Sorba állított MIDI output röviden továbbfuthat. Teljes hibamentességet nem állítunk.
