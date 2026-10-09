# Speed MIDI Editor — Felhasználói útmutató

Ez az útmutató a Speed MIDI Editor kiadott és fejlesztői változatainak munkafolyamatát ismerteti: a Standard MIDI fájlok többsávos szerkesztését, lejátszását és az eredeti Speedy MIDI munkafolyamatát, valamint az új zongorarács-eszközöket. A menüpontok neve buildtől és nyelvi beállítástól függően kissé eltérhet. A kiadás előtt a leírást a végleges verzióval is ellenőrizni kell.

## 1. Mire való a program?

A Speed MIDI Editor MIDI-fájlokat nyit meg, hoz létre, szerkeszt, játszik le és ment. MIDI-szerkesztő és próbaeszköz: nem rögzít és nem kever hangot, és nincs saját hangmintakönyvtára. A lejátszás a kiválasztott MIDI-kimenetre vagy az operációs rendszer által biztosított szoftveres hangszerre kerül.

A projekt Holger Hoffmann Speedy MIDI 1.1 programjának közösségi folytatása. Az eredeti munkafolyamatban MIDI-billentyűzetről vagy képernyőn megjelenő **Mouse Piano**-ról lehet hangokat beírni, több sávot szerkeszteni, ütemtulajdonságokat beállítani és szólamokat kinyerni. Az újabb verziók közvetlen hangrajzolást, törlést, mozgatást, méretezést és a dal nézethez illesztését is kínálják.

## 2. A főablak részei

- **Menüsor és eszköztár:** fájl-, szerkesztési, lejátszási, hangbeviteli és segédfunkciók. Az eszköztárgombok fölött megjelenő súgóbuborék röviden leírja a funkciót.
- **Cell Length:** a hangbeíráshoz és kvantáláshoz használt ritmikai rács felbontása. Egy cella az ütem egy része; nem feltétlenül azonos a hang hosszával.
- **Beat / ütemvonalzó:** az ütem- és pozíciójelölések. Az ütemhez tartozik például az ütemmutató, hangnem, tempó, swing és próba-/szakaszjelölő.
- **Sávinformációs panel:** minden sor egy MIDI-sáv. Látható a neve, hangszere/programja, MIDI-csatornája, hangereje, panorámája és a Solo (S), Mute (M), Record (R) gombok. A MIDI-aktivitásmérő hangaktivitást jelez.
- **Zongorarács:** vízszintesen halad az idő, függőlegesen a hangmagasság. A színes téglalapok MIDI-hangok; balra a billentyűskála oktáv- és hangjelölései láthatók.
- **Görgetősávok és nagyítás:** a vízszintes görgetősávval az ütemek, a függőlegessel a sávlista között lehet mozogni. A mellettük levő vezérlők az időbeli/vízszintes, illetve a hangmagasság szerinti/függőleges nagyítást állítják.

### Navigáció és illesztés

Húzd a vízszintes vagy függőleges görgetősáv csúszkáját a dal vagy a sávlista bejárásához. A nagyítósávokkal, illetve plusz/mínusz gombjaikkal állítsd be, mennyi idő vagy hangmagasság látszódjon. A jobb alsó **Fit Entire Song** gomb (négy, középről kifelé mutató nyíl) úgy állítja a nézetet, hogy a dal időtartománya és sávjai beférjenek a szerkesztőablakba. A **Default Track Height** egységes sáv-magasságot állít be, hogy a sávinformációk következetesen kiférjenek; nem illeszti a teljes dalt vízszintesen. Ezek a parancsok a nézetet, nem a MIDI-adatokat módosítják.

## 3. Dal létrehozása, megnyitása és mentése

- **New, with Wizard:** új dokumentum készítése és sávok hozzáadása a sávvarázslóval. Például a `satb` rövidítés szoprán, alt, tenor és basszus sávokat hoz létre. A varázsló további hangfekvés- és hangszerpéldákat is mutat.
- **New Default Document:** üres dokumentum létrehozása az alapbeállításokkal.
- **Open:** MIDI-fájl megnyitása. Az eredeti program RIFF/RMID-be csomagolt MIDI-fájlokat is kezel.

Az ütemen belüli ütemmutató-váltást vagy törttickes ütemhosszt tartalmazó fájlokat a szerkesztőrács nem tudja kezelni. A megnyitás ilyenkor magyarázattal megszakad; az eredeti fájl és az aktuálisan megnyitott dokumentum megmarad. A páratlan PPQN-felbontású fájlok teljes ütemhatárai továbbra is támogatottak, ha az ütemhossz egész számú tick.

Ugyanez a védelem érvényes a hibás vagy tartományon kívüli ütemmutatókra és az érvénytelen előjegyzésekre. Az importált metronóm- és jelölési adatok megmaradnak mentés, ütemmásolás és undo/redo során, más ütemjellemzők szerkesztésekor is.

A normál Format 1 sávokon tárolt ütemmutatókat és előjegyzéseket is elutasítja a program, mert egyetlen globális vezérlősávrácsot használ. Egy másolat megnyitása előtt másik szerkesztővel helyezd ezeket támogatott vezérlősáv-idővonalra. Az ütemszerkesztés elutasítja a törttickes ütemhosszt, és mentéskor is ellenőrzi az ütemrács újranyithatóságát.

Az importált előjegyzés-váltások pontos tickpozíciója az ütemen belül is megmarad mentés és teljes ütemek másolása/beillesztése során. A szabványos MIDI-előjegyzések hét bé és hét kereszt között támogatottak; a −7…+7 tartományon kívüli értékeket a program elutasítja.

- **Save / Save As:** az aktuális dokumentum mentése, illetve új néven való mentése. Fontos fájlon való kísérletezés előtt készíts másolatot.
- **Save Compatible File:** olyan MIDI-fájl készítése, amelybe a kiválasztott lejátszási sebesség és/vagy swing normál MIDI-eseményként kerül át. Akkor hasznos, ha másik lejátszó nem ismeri a Speed MIDI lejátszás közbeni beállításait.
- **Extract Parts…:** külön MIDI-fájlok készítése kiválasztott sávcsoportokból. Az ablakban inverz szólamok is készíthetők, amelyek az összes meg nem jelölt sávot tartalmazzák.
- **Close / Quit:** dokumentum vagy alkalmazás bezárása. Módosítás esetén ments, amikor a program rákérdez.

A program Standard MIDI fájlokat szerkeszt. Nem garantálja minden gyártóspecifikus vagy hibás MIDI-kiterjesztés megőrzését. Fontos eredetiről mindig dolgozz másolaton, és kritikus esetben egy másik MIDI-lejátszóval vagy szerkesztővel is ellenőrizd a mentett fájlt.

A System Exclusive (SysEx) csomagok megnyitás és mentés során megmaradnak, a több részre bontott csomagokkal együtt. A normál sávokon a kijelölés szerinti másolás, beillesztés, törlés és visszavonás ezekre is hat; a vezérlősáv csomagjai globális fájladatok maradnak. A lejátszás és a MIDI Thru nem küldi ki a SysEx-csomagokat, ezért az ezekben tárolt eszközbeállításokat külön kell alkalmazni.

## 4. Sávok és időtartomány kijelölése

Kattints a zongorarácsra a szerkesztési pozíció beállításához és cella kijelöléséhez. Húzd az egeret a cellákon és/vagy sávokon át egy időtartomány kijelöléséhez. A Szerkesztés és Segédeszközök menü több parancsa a kijelölt tartományra vonatkozik; tömeges művelet előtt ellenőrizd a kijelölt ütemeket és sávokat. A **Select All** a dokumentum szerkeszthető tartományát jelöli ki.

Az **Insert Selected Range** beszúrja az aktuálisan kijelölt tartományt. Az **Insert…** ablakban cellákat, ütemeket vagy sávokat lehet beszúrni, és megadható az ütemek vagy sávok száma. A **Delete** törli a kijelölt tartományt; a **Clear Cells** kiüríti a hangokat, de megtartja a környező rácsot. A két parancs mást csinál; ha nem a várt eredményt kaptad, használd a Visszavonást.

## 5. Hangok szerkesztése a zongorarácson

### Bekapcsolva maradó eszközök

A **Move Notes** (kéz), **Draw Notes** (ceruza) és **Erase Notes** (radír) eszköztárgombok bekapcsolva maradnak, és kölcsönösen kizárják egymást. A kiválasztott eszköz minden művelet után aktív marad, amíg újra rá nem kattintasz vagy másikra nem váltasz. Az aktív kéz, ceruza vagy radír kikapcsolásához **kattints a jobb egérgombbal a zongorarácsra**. A jobb klikk nem módosítja a hangokat, és nem szakít meg egy folyamatban lévő húzást. Ha nincs bekapcsolva ilyen eszköz, a jobb klikknek nincs hatása.

### Hang mozgatása és méretezése

Kapcsold be a kéz eszközt. A hang belsejét balra vagy jobbra húzva módosul az időbeli helye, fel vagy le húzva a hangmagassága. A hang a rácshoz igazodik, a hossza változatlan marad. A hang bal vagy jobb szélét húzva rövidíthető vagy hosszabbítható. A befejezett módosításokat vissza lehet vonni.

### Hang rajzolása

Kapcsold be a ceruzát, majd húzd az egeret a sáv zongorarácsának üres részén. A hangmagasságot a sor, a kezdő- és végpontot a vízszintes rács határozza meg. Az új hang a Beállításokban megadott MIDI velocity értéket kapja. A ceruza gombjára ismét rákattintva, másik eszközre váltva vagy a rácson jobb gombbal kattintva léphetsz ki a ceruzamódból.

### Hang törlése

Kapcsold be a radírt, majd kattints törlendő hangra vagy húzd végig az egeret több hangon. A törlés visszavonható. A radír gombjára ismét rákattintva, másik eszközre váltva vagy jobb klikkel léphetsz ki a radírmódból.

### Kivágás, másolás, beillesztés és visszavonás

Jelöld ki a kívánt cellákat vagy hangokat, majd használd a **Cut**, **Copy** vagy **Paste** parancsot. A **Paste and Scale to Selection** a beillesztett anyagot a kijelölt tartományhoz igazítja. A **Undo** és **Redo** visszavonja, illetve újra végrehajtja az azt támogató műveleteket. Szerkesztés után mentsd a dalt; a vágólap nem helyettesíti a fájlmentést.

## 6. Hangbeírás MIDI-billentyűzetről vagy Mouse Piano-ról

Az eredeti Speedy MIDI munkafolyamat MIDI-billentyűzetről és képernyőn megjelenő **Mouse Piano**-ról is támogatja a lépésenkénti hangbeírást. Válaszd ki a cél-sávot vagy sávokat és egy cellát, állítsd be a cellahosszt, majd szólaltass meg egy vagy több billentyűt, illetve kattints a Mouse Piano billentyűire. A **Write or Extend Note** (Space) paranccsal az éppen lenyomott hangot vagy hangokat írhatod be az aktuális pozícióra. A **Write Multiple Cells** több cellát ír be; az **Extend Note** az előző cellában kezdődő hangot hosszabbítja a jelenlegi cellába. A Write menüben 1–10 cellás beírási és hosszabbítási parancsok vannak.

Ha több sáv van bekapcsolva beírásra és több hangot nyomsz le, a hangok és sávok darabszámának egyeznie kell, hogy a program sávonként ossza szét őket. Egy sávba beírt akkordhoz csak azt az egy sávot hagyd bekapcsolva. A **Record (R)** sávgombbal vagy a Write menü sávjelölő-parancsaival állítható be, mely sávok fogadják a hangbevitelt.

A Mouse Piano megjelenítéséhez válaszd a **View → Mouse Piano** menüpontot. Ha a build tartalmazza, a Preferences/Options ablakban állítsd be a MIDI-eszközt és a normál vagy ütőhangszeres módot. Tartsd lenyomva az **F7**-et (Listen Chord) a lenyomott hangok meghallgatásához; felengedéskor a próba leáll. Az **Escape** leállítja a próbát/lejátszást és felengedi a Mouse Piano hangjait.

## 7. Cellahossz, hangbeírás és ritmus

A bal felső **Cell Length** vezérlő adja meg a lépésenkénti beíráshoz és kapcsolódó műveletekhez használt rács felbontását. Válassz alap hangértéket, majd a Write menüben duplázd vagy felezd, válts triolára vagy duolára, illetve adj meg tetszőleges tuplettet. A rács és a vonalzó az új felbontáshoz igazodik. A cellahossz a szerkesztési rasztert szabja meg; egy hang több cellán is átérhet.

A **Quantize to Cell Raster** a kijelölt hangok időzítését a rácshoz igazítja. A **Scale Note Length…** a kijelölt hangok hosszát arányosan módosítja. A **Split Notes** a kijelölés határán áthaladó hangokat kettévágja; a **Connect Notes** a határ két oldalán levő, egymáshoz illő hangokat összekapcsolja. Nagyobb szakaszon való alkalmazás előtt jelöld ki pontosan a kívánt részt, és ments másolatot.

## 8. Sávvezérlők és tulajdonságok

- **S (Solo):** csak a kiválasztott sáv(ok) hallgatása. Az exkluzív Solo parancs a kijelölt sávot bekapcsolja, a többi sáv Solo állapotát kikapcsolja.
- **M (Mute):** a kijelölt sáv(ok) elnémítása. Az exkluzív Mute parancs a kijelölés némítását állítja, a többi sáv Mute állapotát pedig törli.
- **R (Record):** a sáv(ok) engedélyezése MIDI-hangbeírás céljaként. Az exkluzív Record parancs a többi sávon kikapcsolja a jelölést.
- **Track Attributes…:** sávjellemzők szerkesztése, például sávnév, MIDI-csatorna, hangszer/program, hangerő és panoráma – a párbeszédablakban elérhető mezők szerint.
- **New Track Wizard / Add New Track:** egy vagy több névvel vagy hangszerrel jelölt sáv hozzáadása. A varázsló gyakori kórus- és hangszer-rövidítéseket tartalmaz; dob szólamhoz ütőhangszeres/Drum Set típusú sávot válassz.
- **Default Track Height:** a sávok magasságának egységesítése, hogy a sávinformációk következetesen láthatók legyenek.

A Solo, Mute és Record jelölők a lejátszást vagy a hangbevitelt szabályozzák; MIDI-események törlésére vagy módosítására nem szolgálnak.

## 9. Ütemtulajdonságok és próba-/szakaszjelölők

A kurzor vagy kijelölés alatti ütemhez nyisd meg a **Measure Attributes…** ablakot. Itt megadható az ütemmutató, hangnem, tempó, lejátszási swing és egy opcionális, szöveggel és színnel ellátott próba-/szakaszjelölő. A jelölő segít a zenei részek azonosításában és a próbában. A lejátszási swing és relatív sebesség szerkesztői lejátszási beállítás; a **Save Compatible File** segítségével a kiválasztott opciók normál MIDI-eseményekké alakíthatók más lejátszók számára. Az importált MIDI-tempók pontos értéke és időpontja megmarad, az ütemen belüli váltásoknál is. A tempómező tört BPM-et is elfogad; más ütemtulajdonság módosítása nem kerekíti át az importált tempót.

## 10. Lejátszás és MIDI-beállítás

A külső MIDI-kimenet rövid időre előre megkapja a lejátszási adatokat. Stop, Pause vagy Mute után a már ütemezett hangok még rövid ideig megszólalhatnak (normál esetben hozzávetőleg 0,1 másodpercen belül). Későbbi hangokat a program már nem küld ki; ez a kimenet ütemezési korlátja, a mentett MIDI-fájlt nem módosítja.

A **Play** (F6), **Stop** (F5) és **Return to Start** gombokkal vezérelhető a lejátszás. A kezdőpozíció a Preferences beállítása lehet: a dal eleje, a bal szélen látható ütem vagy a kurzor pozíciója. A kék lejátszási kurzor mutatja az aktuális helyet; bekapcsolt görgetéses lejátszásnál a nézet követi a kurzort. A lejátszási sebesség vezérlő a meghallgatás sebességét módosítja, nem a dal tempóeseményeit.

Az **Options → Preferences** ablakban állítható a program nyelve, a zenei jelölések nyelve, MIDI-bemenet és -kimenet, lejátszási viselkedés, új hangok velocity értéke és az adott buildben elérhető Mouse Piano-beállítások.

Hang hallásához válassz a szintetizátorhoz vagy hangforráshoz csatlakozó kimeneti portot. A MIDI-kimenet kiválasztása önmagában nem telepít vagy biztosít hangkönyvtárat. macOS-en az **Apple Built-in General MIDI** egy választható rendszer-MIDI-cél lehet, ha elérhető; külső hangszer vagy más rendszer-MIDI-cél is kiválasztható.

Ha a bejövő MIDI-adatokat továbbítani szeretnéd a kiválasztott kimenetre, kapcsold be az **MIDI Thru** opciót. Ha egy eszköz nem jelenik meg, csatlakoztatás vagy engedélyezés után zárd be, majd nyisd meg újra a beállításablakot. Néma lejátszásnál ellenőrizd a kimeneti portot, a hangszer hangkimenetét, valamint a sáv Solo/Mute állapotát.

## 11. Segédeszközök

A Segédeszközök parancsai az aktuális kijelölésen vagy sávokon dolgoznak. Tömeges átalakítás előtt készíts másolatot.

- **Transpose:** a kijelölt hangok áthelyezése oktávval, diatonikus fokkal vagy kromatikus félhanggal. A húzós/tartós parancsok interaktív transzponálást, a többlépéses parancsok megadott számú lépést végeznek. Az aktív billentyűzetes művelet az Escape gombbal megszakítható.
- **Split Notes / Connect Notes:** hangok szétvágása vagy megfelelő, szomszédos hangok összekötése a kijelölés határánál.
- **Quantize to Cell Raster:** hangok időzítésének a cellarácshoz igazítása.
- **Scale Note Length…:** a kijelölt hangok hosszának arányos módosítása.
- **Add Swing…:** swing időzítés alkalmazása a hangokra. A párbeszédablak figyelmeztet, hogy ez módosítja az események időzítését; az ütem lejátszási swingje később egyszerűbben ki- és bekapcsolható.
- **Remove Top Voice / Remove Bottom Voice:** a kijelölt rész legmagasabb vagy legalacsonyabb hangjainak/szólamának eltávolítása. Művelet előtt ellenőrizd a kijelölést.

A kinyert szólamok fájlnév-sablonjában a `%s` a dal nevét, a `%t` az adott szólam sávneveit helyettesíti. Üres sávcsoport nem kerül kimentésre.

## 12. Hasznos billentyűparancsok

A parancsok az operációs rendszertől és billentyűzetkiosztástól függően eltérhetnek; az aktuális parancsmenü mutatja a használt billentyűt.

| Művelet | Billentyű |
|---|---|
| Új / Megnyitás / Mentés / Mentés másként | Ctrl+N / Ctrl+O / Ctrl+S / Ctrl+Shift+S |
| Visszavonás / Újra | Ctrl+Z / Ctrl+Y |
| Kivágás / Másolás / Beillesztés / Mind kijelölése | Ctrl+X / Ctrl+C / Ctrl+V / Ctrl+A |
| Lejátszás / Leállítás / Vissza az elejére | F6 / F5 / eszköztár vagy Lejátszás menü |
| Akkord meghallgatása (nyomva tartva) | F7 |
| Lejátszási sebesség | F8 |
| Hang beírása vagy hosszabbítása | Space |
| Hang hosszabbítása | Shift+Space |
| Kvantálás a cellarácshoz | Q |
| Transzponálás oktávonként, nyomva tartva | F2 |
| Diatonikus transzponálás, nyomva tartva | F3 |
| Kromatikus transzponálás, nyomva tartva | F4 |
| Kijelölt sáv Solo / Mute / Record | S / M / R |
| Teljes dal illesztése | Ctrl+F |

## 13. Gyors munkafolyamat első használathoz

1. Nyiss meg egy MIDI-fájlt, vagy hozz létre új dokumentumot a sávvarázslóval.
2. A Beállításokban válaszd ki a MIDI-kimenetet, majd állíts be megfelelő cellahosszt.
3. A kéz, ceruza vagy radír eszközzel szerkeszd a hangokat, vagy írj be hangokat MIDI-billentyűzetről/Mouse Piano-ról.
4. A Solo és Mute segítségével hallgasd meg külön a szólamokat; a Play elindítja, a Return to Start visszaugrik a dal elejére.
5. Mentsd más néven az eredeti megőrzéséhez, majd nyisd meg újra a mentett fájlt és ellenőrizd.
6. Ha más lejátszóban is kell a relatív sebesség vagy swing, használd a Save Compatible File parancsot.

## 14. A jelenlegi változat hatóköre és hibaelhárítás

A Speed MIDI Editor MIDI-hangokat és sávadatokat szerkeszt; nem kottaszerkesztő, hangmunkaállomás vagy beépített szintetizátor. A dob szólam MIDI-ütőhangszeres csatorna-/hangkészlet-konvenciókat használ, a dobhangot pedig a kiválasztott MIDI-cél biztosítja. A lejátszási kompatibilitás a kimeneti eszköztől és annak hangkészletétől függ.

Ha egy fájl nem nyílik meg, ellenőrizd, hogy támogatott Standard MIDI fájl-e, majd próbálj ki egy ismert, működő `.mid` fájlt. Néma lejátszásnál nézd meg a kimeneti portot és a hangforrást. Váratlan szerkesztés esetén azonnal használd a Visszavonás parancsot. Nagyobb segédművelet vagy konvertálás előtt készíts biztonsági másolatot.

A nyilvános változatok a projekt GitHub Releases oldaláról letölthetők. Az útmutató a forrással együtt frissül; a menüpontok neve és elérhetősége a telepített verziótól függhet. Minden új kiadásjelölt saját regressziós és kézi ellenőrzést igényel; egy korábbi kiadás teszteredménye nem igazolja az új változatot.

## 15. A GitHub-os macOS-változat telepítése

Az ajánlott **PKG-telepítő** az **Alkalmazások (Applications)** mappába telepít, rendszergazdai jóváhagyással. Kézi telepítéshez bontsd ki a teljes **ZIP-et**, és másold a **Speed MIDI Editor.app** alkalmazást az Alkalmazások mappába. Csak a projekt [GitHub Releases](https://github.com/RobCZart82/Speed-MIDI-Editor/releases) oldaláról tölts le.

A PKG és az alkalmazás nem rendelkezik fizetős Apple-fejlesztői tanúsítvánnyal; az alkalmazás nincs notarizálva. Ha a macOS a **.pkg** megnyitását blokkolja, zárd be a figyelmeztetést, majd a **Rendszerbeállítások → Adatvédelem és biztonság → Biztonság** résznél válaszd a Speed MIDI Editor mellett a **Megnyitás mindenképp (Open Anyway)** lehetőséget, és erősítsd meg. Ha kéri, azonosítsd magad rendszergazdaként; nyisd meg újra a telepítőt, és használd az alapértelmezett **Macintosh HD** céllemezt. A telepített **.app** esetén csak akkor ismételd meg ezt, ha a rendszer külön azt is blokkolja. Lásd az [Apple útmutatóját](https://support.apple.com/en-gb/102445). A kézi ZIP alkalmazásának első indításakor ugyanez az appjóváhagyás lehet szükséges.
