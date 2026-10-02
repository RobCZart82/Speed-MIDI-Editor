# Speed MIDI Editor – 0.1.3 publikálási terv

Frissítve: 2026. október 2. A felhasználó ebben a beszélgetésben engedélyezte az ellenőrzött javítások beolvasztását és az új kiadás publikálását. A cél a javított 0.1.3 kiadása a tényleges új csomagok ellenőrzése után.

## Kiinduló állapot

- PR44 és PR45 beolvasztva; a javított főág `3c830853e84a7126c72149408ff0c6992a7b880d` Windows-, macOS- és Linux-ellenőrzései sikeresek.
- Az audit öt igazolt hibáját ezek kezelik: nem újranyitható ütemrács mentése, normál sávok előjegyzés/ütemadat-vesztése, páratlan PPQN vágólaphiba, eltolódó előjegyzés-időpont és nem szabványos előjegyzéstartomány. A javításokhoz regressziók készültek.
- A v0.1.2 draft nem publikált, meghaladott jelölt. Tagját és fájljait nem mozgatjuk vagy cseréljük le; az új javítások új verzióba kerülnek.
- A 0.1.3 csomagok még nem készültek el ennél az előkészítési lépésnél. Az aktuális bizonyítékokat a [0.1.3 QA-napló](RELEASE_QA_0.1.3.md) tartalmazza.

## Ellenőrzési sorrend

1. Kiadási előkészítő PR: CMake 0.1.3, Windows resource 0.1.3, workflow v0.1.3, changelog és release notes összhangja; a Névjegy már a központi buildverziót használja.
2. Az előkészítő PR minden ellenőrzése sikeres; beolvasztás, majd a merge commit valamennyi főági Actions-futásának sikeres befejezése. Egyszerre csak egy csomag haladjon ezen a folyamaton.
3. A **Prepare release draft** workflow futtatása az ellenőrzött main-ból v0.1.3 taggal. Windows/macOS és Linux backend, macOS strict sanitizers, csomag- és telepítőtesztek egyaránt sikeresek legyenek.
4. A létrejött exact-commit draft négy csomagjának és SHA256SUMS.txt fájljának visszatöltése és független ellenőrzése: hashek, ZIP-sértetlenség, BUILD_INFO commit/verzió, Windows x64 GUI executable és runtime-ok, macOS Universal 2/app-verzió/aláírás/PKG payload, útmutatók és licencek.
5. A tényleges drafton végzett Windows és macOS alappróbák rögzítése; csak ezután publikálás.

## Kötelező csomagpróbák

Windows x64 és valódi macOS gépen, az új csomag nevét, hashét, BUILD_INFO commitját, OS/CPU-t és MIDI-eszközt rögzítve:

- Telepítő és teljesen kicsomagolt ZIP indítása, lehetőleg fejlesztői Qt nélkül; Névjegy 0.1.3, hiányzó runtime vagy indulási hiba nélkül.
- Hallható MIDI-lejátszás, Stop/Pause, újraindítás és quit/relaunch; a mentett portbeállítás használható.
- Format 0/1 fájlok, note-szerkesztés, másolás/beillesztés, undo/redo, Solo/Mute; mentés más néven, bezárás és újranyitás. Az exact tempó, előjegyzés-időpont és metronómadat megmarad; páratlan PPQN ütemmásolás működik.
- Hibás fájl elutasításakor az aktív dokumentum, undo és eredeti fájl megmarad. Hosszabb, több sávos dal és különböző zoom/kijelzőskálák próbája.
- 0.1.1-ről frissítés külön tesztkörnyezetben; saját dalok és beállítások megmaradnak. Újratelepítés, Windows eltávolítás és macOS Finder/single-instance megnyitás ellenőrzése.

A CI azonos verziójú újratelepítése nem helyettesíti a 0.1.1-ről frissítés próbáját. A kizárólag disposable CI-ra írt telepítőteszt ne fusson a felhasználó élő gépén. Fontos MIDI-fájlokról használjunk másolatot.

## Döntési és publikálási kapu

Minden eredmény PASS, FAIL vagy NOT RUN legyen. Crash, adatvesztés, hibás mentés, indulási/telepítési vagy reprodukálható MIDI-output hiba blokkolja a publikálást. Hiányzó kézi eredményhez az elkészült, már ellenőrzött új draftot kell átadni tesztelésre; ez hiányzó QA bizonyíték, nem új publikálási engedélykérés.

Minimum-OS, Intel Mac runtime, tiszta gép, Windows ARM, külső MIDI és hot unplug lefedettsége hiányos maradhat, de ezt pontosan rögzíteni kell. SysEx fájlmegőrzés van, playback/Thru továbbítás nincs; sorba állított output röviden továbbfuthat. Nincs publisher tanúsítvány és Apple notarizáció; a Mac app ad-hoc aláírt. Ezek ne szerepeljenek bizonyított támogatásként.

Alkalmazás- vagy csomaghiba esetén külön javító PR, zöld PR/main és új verzió/jelölt szükséges. Már létrejött tagot ne mozdítsunk és különböző forrásból készült binárisokat ne cseréljünk ugyanazon név alatt.

Sikeres kapuk után a felhasználó meglévő engedélyével a kiválasztott új draft publikálható. A nyilvános release szövegében a draft állapotra utaló mondatot aktualizálni kell. Ezután kijelentkezett hozzáférésből is ellenőrizendők a letöltések és hashek, majd külön dokumentációs PR frissítse a README letöltési táblázatát, changelogot, readiness és QA állapotot. A dokumentációs PR és főág ellenőrzései is fejeződjenek be; végül az automatikus figyelés leállítható.
