# Conductor-szövegek globális szerkesztése – 2026. november 15.

Kiinduló main: `c73fa4f8e8b5534ba572c107c69ebff456495f36` (PR #56).
Ez a folytatás a DEV-07 cue/lyrics/text részét bővíti; a publikált v0.1.5
csomagjait és tagjét nem módosítja.

## Probléma és eredmény

Az új regresszió a korábbi kódban elbukott: egy teljes ütem törlése megőrizte
a kijelölt cue-eseményt, a későbbit pedig a régi tickpozíción hagyta. A teljes
ütemek vágólapja sem hordozta a külön conductor-listában tárolt szövegeket.

- Ütemtörlés: a [bal;jobb) tartomány időzített cue/lyrics/text eseményei törlődnek,
  a jobb határon és utána levők a törölt idővel visszatolódnak.
- Teljesütem Cut: másolás után az érintett szövegeket kiüríti, a későbbi időpontokat
  nem változtatja. A hangsávok meglévő Cut-viselkedése megmarad.
- Teljesütem Paste: a célterület szövegeit lecseréli, a forrás relatív tickjeit
  a célra helyezi, a későbbi eseményeket az új és régi ütemhossz különbségével tolja.
- A fájlszintű copyright és más, nem ebbe a három típusba tartozó metaadat
  nem kerül automatikusan a conductor-szöveg szerkesztésébe.
- Az Undo/Redo visszaállítja az eredeti adatot és sorrendet.

## Vágólap és kompatibilitás

A V6 MIME-típus a V5-ös mezők után relatív ticket, típust, bájthosszt és nyers
bájtokat hordozó conductor-rekordokat tartalmaz. Az új kliens ezt részesíti előnyben;
a régebbi V1–V5 MIME-payloadok változatlanul rendelkezésre állnak. Régi kliensek
az új conductor-szövegeket nem tudják teljesen átvenni. A Paste and Scale to
Selection továbbra is a hangsávok eseményeire hat; conductor-szöveget nem skáláz.

A beolvasó ellenőrzi a darabszámot, ticktartományt, engedélyezett típust és
hátralévő bájtokhoz viszonyított hosszt, még a dokumentum módosítása előtt.
Negatív, túl nagy vagy csonka rekordok elutasítást eredményeznek. A beillesztés
előellenőrzése az alkalmazás importálható tickhatárát használja; a pozitív
idővonal-eltolás conductor-eseményeit is ellenőrzi.

## Ellenőrzés

A normál editor_regression CTest-csoport lefedi a törlést és Undo/Redo-t,
a V6 Copy/Paste-ot és V5 visszaesést, Unicode szöveget és azonos tickű rekordokat,
a teljesütem Cut-ot, valamint a 3/4 és 5/4 ütemek 4/4-be illesztésekor előálló
negatív és pozitív időhossz-különbséget. A mentett állapot újranyithatósága és
az Undo/Redo byte-pontos visszaállítása is ellenőrzött. Öt hibás V6 rekordváltozat
nem módosíthatja a célfájlt vagy az undo-előzményt. Külön próba ellenőrzi a
fájlszintű copyright megőrzését és a conductor-idővonal túlcsordulását okozó
beillesztés elutasítását.

Helyi build: macOS arm64, Qt 6.11.2, Debug ASan/UBSan halt-on-error. A végleges
PR és main platformos ellenőrzéseit a beolvasztás előtt/után külön követjük.
A macOS kiadási CI-ben a már meglévő IPC-teszt fix 2,2 másodperces várakozása
versenyhelyzetet jelzett a lezárási időzítővel. A teszt most konkrét socket-
és nyugtázási feltételekre vár, legfeljebb 5 másodpercig, pontos időzítővel;
a kiszolgáló 2 másodperces timeoutja és a natív CI-kötelezettség megmaradt.
A külső CTest-határ 30 másodperc, hogy a több, külön határolt lépés beleférjen.
Ez nem tesztkihagyás vagy az IPC-kiszolgáló határidejének lazítása.
Új telepítő, minimum-OS vagy fizikai MIDI-eszköz kézi próbája most nem történt.

## Megmaradó korlátok

A conductor SysEx, track-end és más globális metaadattípusok általános
insert/delete/copy/paste kezelése nem része ennek a változtatásnak. A fejléc-
és copyright-jellegű információk időzítési szabályait nem szabad a dalszövegre
és cue-pontokra automatikusan kiterjeszteni. A DEV-07 ezért részben lezárt,
a hardveres és minimumverziós lefedettség további feladat.
