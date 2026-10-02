# Speed MIDI Editor – 0.1.3 kiadási állapot

Frissítve: 2026. október 2. A [0.1.3 kiadás](https://github.com/RobCZart82/Speed-MIDI-Editor/releases/tag/v0.1.3) nyilvános. A felhasználó az aktuális Windows- és macOS-telepítő és program kipróbálása után nem tapasztalt látható vagy hallható hibát, és kifejezetten engedélyezte a publikálást. A 0.1.2 meghaladott draft marad; tagját és csomagjait nem változtattuk.

## Elvégzett lépések

- PR44/45: az öt igazolt adatmegőrzési, ütemrács-, előjegyzés- és vágólaphiba javítása regressziós tesztekkel.
- PR46: verzió és kiadási dokumentáció előkészítése; minden PR és főági ellenőrzés sikeres, egymást követő csomagokkal.
- A kiadási commit `613a97b20435bf3338d3dd8ffc281e8ea26c196b`, a [37046563168 kiadási futás](https://github.com/RobCZart82/Speed-MIDI-Editor/actions/runs/37046563168) minden jobja sikeres.
- Négy csomag és SHA256SUMS független ellenőrzése: hashek, ZIP CRC, BUILD_INFO, Windows runtime és x64 GUI PE, macOS Universal/verzió/codesign/PKG tartalomegyezés.
- A tényleges telepítők és programok felhasználói kipróbálása mindkét platformon, majd publikálás; az öt nyilvános fájl hitelesítés nélküli letöltése és hash-egyezése sikeres.

## Bizonyítékok és korlátok

A [QA-napló](RELEASE_QA_0.1.3.md) külön jelöli az automatizált, független és felhasználó által jelentett eredményeket. Az általános felhasználói próbából nem állítunk külön bizonyított 0.1.1-frissítési, mentés/újranyitási, konkrét OS/CPU/MIDI-eszköz vagy teljes kézi mátrixeredményt.

Minimum OS, Intel runtime, tiszta gépek, Windows ARM, külső MIDI és hot unplug lefedettsége hiányos. SysEx fájlmegőrzés van, playback/Thru továbbítás nincs; sorba állított output röviden továbbfuthat. Nincs publisher tanúsítvány vagy Apple notarizáció. Élő felhasználói gépen a CI telepítőtesztet nem futtattuk.

A dokumentációs lezáró PR frissíti a letöltési linkeket és kiadási állapotot. Ennek PR- és főági ellenőrzései után a kiadás követése befejezhető. Már létrejött tagot nem mozgatunk és különböző forrásból készült binárisokat nem cserélünk azonos verziónév alatt.
