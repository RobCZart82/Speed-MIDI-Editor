# Speed MIDI Editor publikálási terv

Ellenőrizve: 2026. október 2. A cél a 0.1.2 kiadás publikálása a már elkészült draftból, a tényleges csomagokon végzett kézi ellenőrzések után. A terv a maintainer és a további Codex vagy ChatGPT Work feladatok számára rögzíti a hátralévő munkát. A dokumentum létrehozása nem jelent publikálási engedélyt.

Kiadási formátumok: Windows x64 EXE telepítő és Manual Install ZIP; macOS Universal 2 PKG telepítő és Manual Install ZIP; közös SHA256SUMS.txt. DMG nem készül. Linuxon backend tesztek futnak, Linux-csomag nem része ennek a kiadásnak.

## Ellenőrzött kiinduló állapot

A [0.1.2 draft](https://github.com/RobCZart82/Speed-MIDI-Editor/releases/tag/untagged-b7181cfae17b650b4771) elkészült, nem publikált. A csomagok forrása és a v0.1.2 tag commitja: `a3dbbc39d4e53d29240c3fa9d74f7b0e25bf8a89`. Későbbi main-commitok önmagukban nem kerülnek bele ezekbe a fájlokba. A tesztelés során mindig a csomag BUILD_INFO.txt fájlját és hashét kell azonosítani.

| Ellenőrzés | Állapot és bizonyíték |
| --- | --- |
| Hibajavítások és kiadás előkészítése | [PR40](https://github.com/RobCZart82/Speed-MIDI-Editor/pull/40), [PR41](https://github.com/RobCZart82/Speed-MIDI-Editor/pull/41), [PR42](https://github.com/RobCZart82/Speed-MIDI-Editor/pull/42) beolvasztva |
| Helyi Windows Release build és regressziók | PASS, 13/13 teszt; natív Microsoft MIDI-output és tényleges Névjegy-verzió teszt is sikeres |
| Beolvasztott main Windows csomag és telepítőtesztek | [PASS](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/36992821353) |
| Beolvasztott main macOS Universal 2, PKG és strict sanitizers | [PASS](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/36992821335) |
| Beolvasztott main Linux backend sanitizers | [PASS](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/36992821277) |
| Az exact commitból újraépített draft és feltöltés | [PASS](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/36993602966) |
| Feltöltött fájlok visszaellenőrzése | PASS: mind az öt asset letöltve, méret és SHA256 ellenőrizve; ZIP CRC, BUILD_INFO verzió és commit, licencek és útmutatók ellenőrizve |
| Az új draft kézi indítása, hallható playback és mentés | NOT RUN ebben az előkészítésben; korábbi builden végzett felhasználói próbák nem helyettesítik |

A korábbi hibajavítások már benne vannak a draftban: hibás vagy nem támogatott ütemmutatók és előjegyzések biztonságos elutasítása; metronóm- és jelölési bájtok megőrzése mentés, szerkesztés, másolás és undo/redo során; a Névjegy verziószámának központi build-adathoz kötése. Ezeket nem kell újra implementálni.

## Első lépés a tesztcsomag és az eredménynapló

- [ ] A maintainer töltse le a draft tényleges EXE, PKG és két ZIP fájlját, valamint a SHA256SUMS.txt fájlt. Ellenőrizze a hasheket a tesztgépen is.
- [ ] Készítsen külön másolatot a tesztelt MIDI-fájlokról. Kézi teszt ne írja felül az egyetlen eredeti példányt.
- [ ] Minden teszthez rögzítse az OS pontos verzióját, az architektúrát, a csomag nevét/hashét, a BUILD_INFO commitját, a MIDI-eszközt, a lépéseket és az eredményt.
- [ ] Használjon PASS, FAIL vagy NOT RUN állapotot. FAIL esetén csatoljon reprodukciót és szükség szerint képet vagy hibalogot; érzékeny felhasználói fájl ne kerüljön a repository-ba.

Javasolt eredménynapló a repository `docs` mappájában:

| Dátum és tesztelő | OS és CPU | Csomag és SHA256 | Teszt és MIDI-eszköz | Eredmény | Bizonyíték vagy hiba |
| --- | --- | --- | --- | --- | --- |
| Kitöltendő | Kitöltendő | Kitöltendő | Kitöltendő | NOT RUN | Kitöltendő |

## Második lépés Windows kézi ellenőrzés

Ezeket az új drafttal kell elvégezni. A kötelező alappróba Windows 11 x64-en történjen, lehetőleg fejlesztői Qt nélküli gépen vagy külön tiszta környezetben.

- [ ] EXE telepítés normál felhasználóként; Start menü indítás. Nincs külön konzolablak, hiányzó DLL vagy indulási crash; a Névjegy 0.1.2-t mutat.
- [ ] A Manual Install ZIP teljes kicsomagolása és önálló indítása másik mappából.
- [ ] Microsoft GS Wavetable Synth, illetve használható Microsoft MIDI Mapper kiválasztása; hallható note playback, Stop/Pause, újraindítás és kilépés. Nem használható natív eszköz esetén a konkrét körülményt rögzíteni kell.
- [ ] MIDI-output beállítása után quit/relaunch: a mentett port és beállítások használhatók, az alkalmazás nem omlik össze.
- [ ] Telepítő újrafuttatása és 0.1.1-ről frissítés külön tesztkörnyezetben; felhasználói dalok és beállítások megmaradnak. Eltávolítás működik, saját MIDI-fájl nem törlődik.

Az automatizált telepítőteszt már PASS, de a grafikus indítás és hallható MIDI-output külön kézi feladat. Az élő fejlesztői gépen ne fusson a kizárólag disposable CI-ra írt telepítőteszt-script.

## Harmadik lépés macOS kézi ellenőrzés

A kötelező alappróba egy valódi Macen történjen; az OS és az Intel vagy Apple Silicon architektúra legyen rögzítve. Mindkét architektúra futásellenőrzése ajánlott; az Universal 2 binárisellenőrzés önmagában nem bizonyítja az Intel gépen működő playbacket.

- [ ] PKG telepítés, majd indítás az Applications mappából; a Névjegy 0.1.2-t mutat. A rendszer elsőindítási jelzése és az alkalmazott normál jóváhagyási lépések legyenek dokumentálva.
- [ ] Manual Install ZIP kipróbálása fejlesztői Qt nélkül; a teljes app bundle másolása Applications-be és indítása.
- [ ] Apple Built-in General MIDI, ha elérhető: output kiválasztása, hallható playback, Stop/Pause, quit/relaunch és portbeállítás megőrzése.
- [ ] MIDI-fájl megnyitása Finderből, mentés más néven, bezárás és újranyitás; single-instance működés ellenőrzése.
- [ ] PKG újratelepítés és 0.1.1-ről frissítés külön tesztkörnyezetben; dalok és preferenciák megmaradnak, az új alkalmazás indul.

## Negyedik lépés közös szerkesztési és fájlintegritási próba

Ezeket legalább a Windows és a macOS alappróbában is végre kell hajtani.

- [ ] Format 0 és Format 1 fájl megnyitása, több track és csatorna, tracknév, tempó, ütemmutató és előjegyzés ellenőrzése.
- [ ] Note rajzolás, mozgatás, átméretezés és törlés; másolás/beillesztés, undo/redo; track hozzáadása/törlése és Solo/Mute.
- [ ] Piano-roll vízszintes sorhatárok ellenőrzése, különösen F/E mellett, különböző zoomokkal és Windows kijelzőskálával.
- [ ] Save As, bezárás és újranyitás; az események és időzítés összevetése másik MIDI-olvasóval. Ellenőrizendő az exact tempo és a 6/8-as metronóm/jelölési adatok megőrzése is.
- [ ] Hibás/csonka fájl, nem támogatott ütemen belüli váltás és tartományon kívüli ütemmutató megnyitása: érthető hiba, az aktuális dokumentum, az undo és az eredeti fájl változatlan marad.
- [ ] Egy hosszabb és egy több trackes dal szerkesztése, lejátszása és mentése; nincs fagyás, crash vagy elromló görgetés.

A parser és a metronómadatok részletes regressziói már automatizáltak. A kézi kör célja a kiadott csomag felhasználói folyamatainak ellenőrzése.

## További lefedettség és dokumentált korlátok

Windows 10 1809+, macOS 13, Intel Mac, tiszta gép, külső MIDI input/output, eszköz kihúzás/visszacsatlakoztatás és Windows ARM emuláció még nem teljesen ellenőrzött. Amelyik környezet rendelkezésre áll, azon külön eredményt kell rögzíteni. Ami nem tesztelhető, maradjon NOT RUN, és a release notes ne állítsa ellenőrzött támogatásnak. A minimum-OS és architektúra lefedettség fennmaradó hiányát a maintainernek publikálás előtt tudatosan el kell fogadnia vagy szűkítenie kell a támogatási állítást.

Ezek jelenleg dokumentált korlátok, nem automatikusan új feature-feladatok:

- SysEx megőrződik a fájlokban, de playback és MIDI Thru nem továbbítja.
- A már sorba állított MIDI-output rövid ideig folytatódhat Stop/Pause/Mute után.
- Nem támogatott ütemrácsokat a program biztonságosan elutasít.
- Nincs publisher tanúsítvány vagy Apple notarizáció; a Mac app ad-hoc aláírt. Ezt a telepítési útmutatóban és release notes-ban egyértelműen jelezni kell.

E korlátok megszüntetése külön fejlesztési döntés. A 0.1.2 körébe új feature nem szükséges, ha a fenti alappróbák sikeresek és nincs új blokkoló hiba.

## Javítási folyamat sikertelen teszt esetén

1. A draft maradjon publikálatlan. Crash, adatvesztés, hibás mentés, reprodukálható MIDI-output hiba, indulási vagy telepítési hiba blokkolja a kiadást.
2. Codex vagy a fejlesztő a pontos csomagból és reprodukcióból induljon ki. Előbb igazolja a hibát, majd külön branch-en, kis logikus commitokkal javítsa; ahol automatizálható, adjon hozzá a hibás kódon megbukó regressziót.
3. A PR valamennyi szükséges ellenőrzése legyen zöld, a review észrevételei rendezve; ezután beolvasztás és main ellenőrzés következzen.
4. A v0.1.2 tag már létezik és az ellenőrzött csomagokra mutat. Ne mozdítsuk el, és ne csereberéljük más forrásból készült binárisokra a jelöltet. Alkalmazás- vagy csomagjavítás esetén új verzió és új draft kell, például 0.1.3; CMake, Windows resource és release notes együtt frissüljön.
5. A **Prepare release draft** workflow a javított main commitból készüljön, majd új hash- és csomagellenőrzés és az érintett kézi próbák következzenek. Korábbi eredményt csak változatlan csomagra szabad újra felhasználni.

Csak repository-dokumentáció módosítása nem változtatja meg a fagyasztott 0.1.2 binárisokat. Ha a csomagban szállított útmutatót vagy telepítési viselkedést kell javítani, az új csomag ugyanúgy új jelöltet és ellenőrzést igényel.

## Publikálás előtti ellenőrzőlista

- [ ] Windows és macOS alappróbák, közös fájlintegritási és szerkesztési próbák eredményei rögzítve; nincs nyitott blokkoló hiba.
- [ ] A hiányzó környezetek és ismert korlátok szerepelnek az eredménynaplóban és release notes-ban; a maintainer áttekintette őket.
- [ ] A draft tagja, commitja, csomagverziói és BUILD_INFO egyeznek; az exact-commit release workflow zöld. A későbbi main történetét nem keverjük a draft tartalmával.
- [ ] Az öt elvárt asset jelen van: EXE, PKG, két Manual Install ZIP és SHA256SUMS.txt. Nincs DMG, hiányos feltöltés vagy hibás hash.
- [ ] Release notes: ténylegesen beépült javítások, rendszerkövetelmények, telepítés, aláírási állapot, bizonyított teszteredmények és fennmaradó korlátok pontosak. Ne maradjon publikálás után elavult „not published yet” megfogalmazás.
- [ ] GPL, PortMidi és további third-party notices, angol/magyar útmutatók a csomagokban; a címkézett forrás elérhető.
- [ ] A maintainer kifejezetten engedélyezi az ellenőrzött jelölt publikálását. A zöld Actions vagy ez a terv önmagában nem ez az engedély.

## Publikálás és közvetlen utóellenőrzés

1. Az engedély után a kiválasztott, ellenőrzött draft publikálása a meglévő taggal és assetekkel. Korábbi publikált kiadások, tagek és letöltések maradjanak változatlanok.
2. A nyilvános release-oldal megnyitása kijelentkezett környezetben is; mind a négy csomag és a checksum fájl letöltési linkjének ellenőrzése, letöltés utáni hash-ellenőrzéssel.
3. Külön dokumentációs PR-ben a README letöltési táblázatának átállítása az új nyilvános kiadásra. A RELEASE_READINESS, release notes és changelog aktuális állapotának frissítése a tényleges publikálási dátummal és QA bizonyítékokkal.
4. A publikus kiadás URL-jének és az eredménynaplónak rögzítése. Felhasználói hibajelzés esetén a csomag neve, hash, OS, MIDI-eszköz és reprodukció alapján induljon az új javítási kör.

Ha publikálás után súlyos hiba derül ki, a release notes kapjon pontos ismert-hiba jelzést, és új patch-verzió készüljön. Már publikált binárisokat ne írjunk felül más tartalommal ugyanazon a néven.

## Következő konkrét feladat

A maintainer az új 0.1.2 EXE/ZIP és PKG/ZIP csomagokon végezze el a fenti alappróbákat és küldje vissza az eredményeket. Codex ezután a FAIL eredmények javítását vagy a publikálás előtti utolsó ellenőrzést végezze el. A kiadás addig draft marad.
