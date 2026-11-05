# Parry Hotter — Gothic 1 🪄

**Wersja 0.12 DEV — HUD Union, kamera zza ramienia, poprawione namierzanie i bezpieczniejsze sterowanie PPM → LPM.**

To nadal **prototyp źródłowy**, a nie kompletnie sprawdzony mod `.mod`. Nie ma jeszcze modelu Harry'ego z `Komnaty Tajemnic` ani fizyki ragdoll. Sterowanie PPM + LPM i HUD mają skompilowaną wtyczkę Union, ale nie zostały jeszcze potwierdzone w uruchomionym Gothic 1.

### Diagnostyka bez ręcznego szukania plików

Po próbie uruchomienia kliknij dwukrotnie **`SPRAWDZ_PARRY_HOTTER.bat`**. Utworzy `ParryHotter_diagnostics.txt` obok pliku BAT i sprawdzi w trybie tylko do odczytu: profile DEV/PLAY, plik `.mod`, kompilację zaklęć, polskie OU.bin, wszystkie 14 ikon oraz ustawienia myszy. Możesz przesłać raport, jeśli gra zachowuje się nieprawidłowo. Raport **nie sprawdza rozgrywki**, fizyki ani ruchów Harry'ego.

## Etap 4g: ścisła kolejność PPM → LPM (DEV 0.12)

Poprawiłem ważny błąd prototypu: przy równoczesnym wciśnięciu obu przycisków myszy HUD mógł błędnie uznać to za rzut. Od teraz kolejność jest jednoznaczna:

- **PPM** rozpoczyna przygotowanie różdżki.
- **LPM należy puścić podczas celowania**, a następnie kliknąć na NPC — czar może zostać rzucony dopiero na nowym kliknięciu, kiedy PPM już był trzymany.
- Przytrzymywanie LPM nie powtarza zaklęcia, a jednoczesne naciśnięcie PPM + LPM nie rzuca czaru.
- Wyjście do ekwipunku, cutscenki, pauza lub zapis **przerywa przygotowanie**. Po powrocie należy puścić i ponownie nacisnąć PPM, zanim zostanie przygotowane nowe zaklęcie. Zapobiega to przypadkowym czarom po zamknięciu menu.
- Zmiana zaklęcia klawiszami 1–6 w czasie celowania działa tylko wtedy, gdy aktywny jest dozwolony stan PPM. Wprowadziłem testy kombinacji wejść (w tym przycisków wciśniętych przy powrocie z menu).

**Dla użytkownika:** nie zmienia to sześciu stale widocznych ikon ani celownika. Nadal nie mamy potwierdzenia zachowania myszy i animacji w prawdziwej instalacji Gothic 1, mimo pomyślnego przejścia testów C++ i kompilacji DLL.

## Etap 4f: precyzyjne celowanie w NPC (DEV 0.11)

Wcześniejsza wtyczka wymagała jednocześnie promienia ze środka kamery **i** identycznego wyniku ze standardowego systemu focus Gothica. Gdy engine namierzał sąsiedniego NPC, symbol czaru znikał lub rzut był blokowany mimo poprawnego celownika.

- Nowy kod pobiera **najbliższy obiekt bezpośrednio z promienia środka kamery**. Jeśli pierwszym trafieniem jest żywy NPC w tym samym świecie, w zasięgu 19 metrów, może zostać celem; ściany, przedmioty i martwe postacie blokują namierzanie.
- W chwili rzucania PPM + LPM plugin wywołuje `SetFocusVob` z NPC z celownika, aby natywny `Spell_Cast` korzystał z właściwego celu, a także zapamiętuje tę postać dla efektów fizycznych Levicorpus i Bumbardo.
- **F10** pokazuje teraz także status celowania: NPC w celowniku, przeszkoda/brak NPC, poza zasięgiem, martwy NPC, brak kamery lub brak księgi magii. Ułatwia to ustalenie przyczyny, dla której LPM nie rzuca czaru.
- Dodano niezależny test stanów celowania, a natywna biblioteka nadal podlega kompilacji w CI.

**Granice:** nowa metoda korzysta z osi kamery i kolizji bryły VOB, nie z pikselowego trafiania samej tekstury. Nie kopiowaliśmy oryginalnej kamery HP2. Wpływ `SetFocusVob` na faktyczne trafienia pocisków i zachowanie rozmów z NPC trzeba sprawdzić w uruchomionym Gothicu 1.

## Etap 4e: diagnostyka fizyki w grze (DEV 0.10)

Dodałem **opcjonalny panel F10**, który umożliwia sprawdzenie podczas gry, czy rzeczywiste trafienie Bumbardo/Levicorpus dociera do wtyczki i dlaczego NPC nie został wybity. **Domyślnie panel jest wyłączony.**

Panel pokazuje: **rozpoznane trafienia**, **nadane impulsy**, **wykryte lądowania**, a także liczby przypadków **braku aktywnej fizyki**, **braku `zCRigidBody`** i **przeszkód na drodze**. Dane nie są wysyłane ani zapisywane do pliku. Po ponownym naciśnięciu F10 panel znika.

- Jeden rzeczywisty trafiony NPC może dać **jedno zgłoszenie diagnostyczne na czar** — wielokrotne obrażenia z tego samego pocisku nie powinny zawyżać liczników.
- Otwarcie ekwipunku, wczytywanie i scenki przerywają obserwację trwającego lotu. Zapobiega to odtwarzaniu animacji lądowania podczas menu.
- Dodano testy `union/tests/test_impact_telemetry.cpp` obok istniejących testów czasu lotu i przeszkód.
- Wciąż należy sprawdzić w prawdziwej instalacji Gothic I, czy konkretne NPC mają natywną fizykę. Zielona kompilacja **nie jest takim testem**.

**Test lokalny:** Po `Reparse Scripts` i ponownym zbudowaniu `ParryHotter.mod` uruchom profil PLAY, naciśnij **F10**, wybierz **2** lub **5**, przytrzymaj PPM i kliknij LPM podczas celowania w człowieka. Zwróć uwagę na zmiany liczników. Panel F10 jest tylko dodatkową diagnostyką, sześć ikon i celownik powinny pozostać widoczne niezależnie od niego.

## Etap 4d: bezpieczne lądowanie i powrót do AI (DEV 0.9)

W `union/hud/FlightRecovery.hpp` dodałem mały automat stanów, który obserwuje NPC po udanym fizycznym impulsie. Nie wstrzymuje NPC ani nie nadpisuje jego pozycji.

- Przy trafieniu rozpoczęta zostaje obserwacja na **maksymalnie 4,6 sekundy**. Wykrycie lądowania wymaga co najmniej 450 ms od wybicia, dwóch kolejnych potwierdzeń kontaktu z podłożem oraz niewielkiej prędkości pionowej. Odbicie od ziemi resetuje licznik.
- Wykrywanie podłoża korzysta z krótkiego promienia pionowo w dół, sprawdzenia odległości do podłogi i **normalnej wielokąta**. Pionowe ściany nie są zaliczane jako podłoże.
- Po lądowaniu **Bumbardo Maximus** próbuje jednorazowo uruchomić istniejącą animację `T_STAND_2_LIGHTNING_VICTIM`. **Levicorpus** pozostaje pod kontrolą swojego stanu `ZS_PH_Levicorpus` (powrót do rutyny po około czterech sekundach).
- Jeśli NPC umrze, zmieni się świat, gra zacznie zapis lub upłynie limit czasu, referencja jest zwalniana. Nie wymuszamy ani animacji, ani dodatkowej fizyki w tych przypadkach.
- Podczas zapisu wtyczka tymczasowo ustawia `PH_UNION_LOADED=0`, żeby zapis później otwarty bez pluginu nie zapamiętał nieprawidłowo trybu PLAY. Po zapisie wraca do trybu Union.
- Nowy test `union/tests/test_flight_recovery.cpp` sprawdza normalne lądowanie, odbicia, przedwczesne wyzwolenie, przerwanie i timeout.

**Nie jest gotowe:** obrócenie NPC głową w dół, animacje własnego szkieletu HP2 oraz pełny ragdoll. W prawdziwym Gothic 1 fizyczne wybicie nadal zależy od tego, czy NPC ma aktywny `zCRigidBody`; w przeciwnym razie zachowa tylko bazowe reakcje skryptowe.

## Etap 4c: pierwsza fizyka trafień NPC (eksperymentalna)

- Wtyczka Union przechwytuje **rzeczywiste trafienie** przez natywny `oCNpc::OnDamage_Hit`, dopiero po wykonaniu oryginalnej procedury obrażeń Gothic 1. Nie porusza NPC za samo kliknięcie LPM ani za najazd celownikiem.
- Przed impulsem weryfikuje: **żywy ludzki NPC, gracz jako atakujący, oczekiwany identyfikator czaru, aktualny cel rzuconego zaklęcia** oraz brak wcześniejszego impulsu z tego samego rzutu.
- **Levicorpus:** ograniczony impuls do góry (maksymalnie 165 jednostek prędkości z aktualnego stanu). **Bumbardo Maximus:** mocniejszy impuls w górę i od gracza (maksymalnie 285 w pionie i około 155 w poziomie).
- Silnik sprawdza **przeszkody nad NPC i przed nim**; przy niskim suficie nie wywołuje wybicia pionowego, a przy przeszkodzie z przodu nie dodaje pędu poziomego.
- **Bez przełączania trybu kolizji i grawitacji:** dodatkowy impuls działa wyłącznie na NPC, który *już ma aktywne* natywne ciało fizyczne `zCRigidBody` i `physicsEnabled`. Jeśli go nie ma, pozostają skryptowe reakcje oraz oryginalne efekty Gothica. To celowo ostrożne zachowanie, a nie gwarancja wyskoku każdego NPC.
- Podczas zapisu, zmiany lokacji i wczytywania plugin usuwa zapamiętany cel i stan oczekującego pocisku.
- **Ochrona zwykłych zaklęć:** symbol `PH_UNION_LOADED` wyłącza starszą identyfikację wszystkich czarów gracza jako Parry Hotter w profilu PLAY. W DEV bez DLL pozostaje podstawowy tryb skryptowy.

**Braki:** nie ma jeszcze rzeczywistego odwrócenia NPC do góry nogami ani długiego zawieszenia w powietrzu; obracanie podczas Bumbardo i pełny ragdoll wymagają dodatkowej integracji z animacjami. Samo pozytywne przejście kompilacji i testów nie dowodzi, że wszystkie postacie mają aktywną fizykę w Gothic 1.

## Etap 4b: rozpoznawanie czaru przy trafieniu i ochrona podstawowego Gothica (v0.7)

- **Wtyczka Union zapisuje numer rzucanego czaru** do symbolu Daedalus `PH_SELECTED_SPELL` przed faktycznym `Spell_Cast()`. Skrypty trafienia odróżniają dzięki temu wybrany czar Parry od innych zaklęć korzystających z tych samych bazowych efektów Gothic 1.
- **Bumbardo Maximus:** dodatkowa animacja wstrząsu `T_STAND_2_LIGHTNING_VICTIM` po zdarzeniu trafienia człowieka; podstawowy odrzut wykonuje nadal silnik Stormfist. Nie ma jeszcze własnej fizyki wyrzutu wysoko w powietrze ani obracania NPC.
- **Bez podwójnych efektów:** instalator aktualizuje starsze haki w `B_AssessMagic.d` bez dokładania drugiej reakcji. Sprawdzane testami regresji.
- **Brak zmiany globalnych obrażeń:** wcześniejsze wersje ustawiały `SPL_DAMAGE_BREATHOFDEATH=1` wszystkim NPC. Nowy instalator nie narusza tej stałej. Jeśli stara wersja zmieniła ją, a istnieje kopia `.parryhotter.bak`, oryginalna wartość jest przywracana. W efekcie Avada Kedavra może zabić słabszego NPC wcześniej niż zakończy się animacja — osobny system śmierci pozostaje do zrobienia.
- **Ochrona przed starymi skryptami:** pakowanie `.mod` wymaga obecności `PH_SELECTED_SPELL` w `Gothic.dat`, a `SPRAWDZ_PARRY_HOTTER.bat` raportuje jego brak. Po aktualizacji trzeba zrobić **Reparse Scripts** i przebudować lokalne `.mod` przed testem profilu PLAY. Krótkie okno identyfikacji czaru wygasa po 7 sekundach i resetuje się przy zmianie świata.
- **Tryb DEV bez Union:** dla kompatybilności nadal używa dawnych warunków `Npc_IsPlayer(other)`. Pełna kontrola slotów wymaga profilu **PLAY** z wtyczką, nie samej podmiany run.

**Weryfikacja:** testy Pythona, polskich skryptów i niezależnego kompilatora Daedalus; kompilacja Union na Windows. Brak testu działania w rzeczywistym silniku Gothic 1.

## Etap 4/5: bezpieczniejsze zaklęcia i widoczna gotowość HUD (DEV)

- **Levicorpus:** po trafieniu człowieka z czaru IceCube uruchamia się osobny stan `ZS_PH_Levicorpus` na około 4 sekundy, bez oryginalnych obrażeń zamrażania. NPC ma potem powrócić do zwykłej rutyny. **To na razie zastępcza animacja, nie fizyczne zawieszenie głową w dół.**
- **HUD:** dla sześciu slotów dodano osobne odnowienie (Flipendo 0,6 s; Levicorpus 3,6 s; Rictusempra 2,5 s; Expelliarmus 1,9 s; Bumbardo Maximus 2,8 s; Avada Kedavra 5,2 s).
- **Mana:** mod odczytuje stan many z Gothic 1; nie pobiera jej drugi raz. Niedostępne ikony mają być przygaszone. Gdy gra pobierze manę w trakcie przygotowania zaklęcia, mod nie powinien blokować już przygotowanego rzutu.
- **Testy:** dodałem niezależne testy C++ odnowienia i kosztów many, a istniejący test kompilatora Daedalusa obejmuje nowy stan Levicorpus. Kwestia faktycznej animacji i obsługi przez Gothic 1 wymaga testowania w silniku.

**Czego to jeszcze nie zapewnia:** prawdziwej lewitacji 3D, fizyki eksplozji Bumbardo, komicznych animacji Rictusempra ani gotowego modelu Harry'ego z peleryną.

## Etap 3/5: różdżka w ręce (nowe — Gothic 1 Union, eksperymentalne)

Dodałem osobny model różdżki:
- **Źródło oryginału HP2 PC:** [Wand — The Models Resource](https://models.spriters-resource.com/pc_computer/harrypotterthechamberofsecrets/asset/291995/). To inny model niż Harry na miotle. Plik zawiera `skWandmesh.obj` i `skWandTex0.png`. Instalator próbuje pobrać ZIP i szuka go w `Downloads`, ale oryginalne pliki pozostają wyłącznie lokalne.
- **Gdy oryginał jest niedostępny:** `tools/wand_mesh.py` generuje własną, niskopoligonową drewnianą różdżkę. Wystarczy Python z biblioteką standardową — bez Blendera. Eksportuje `PH_WAND.3DS` (siatka Gothic) oraz `PH_WAND.TGA` (tekstura), a jeśli uda się pobrać oryginał, zachowuje kształt i teksturę z HP2.
- `tools/setup.py` generuje różdżkę automatycznie, `tools/package_mod.py` dołącza ją do lokalnego `ParryHotter.mod`, a `SPRAWDZ_PARRY_HOTTER.bat` wykrywa oba pliki.
- Wtyczka `union/Plugin/plugin.cpp` **próbuje** przy przytrzymaniu PPM podpiąć model do wolnego gniazda `ZS_RIGHTHAND` człowieka. Po zwolnieniu PPM odpina go, nie nadpisując prawdziwej broni NPC.
- Funkcjonalność **została skompilowana dla Gothic 1**, ale pozycja różdżki i poprawność jej pokazywania podczas animacji **nie zostały jeszcze sprawdzone w uruchomionej grze**.

**Ważne:** ten etap nie oznacza, że ręka i peleryna Harry'ego są już animowane jak w HP2. Różdżka korzysta z istniejących animacji rzucania czarów Gothica.

## Etap 2/5: wygląd Harry'ego, szkolna szata i peleryna

**Zaimplementowane w repozytorium:**
- Parry Hotter zaczyna nową grę w **szacie Maga Ognia z Gothic 1** (tymczasowa długa szata z animacjami Gothica, a NIE oryginalny Harry).
- Instalator szuka oryginalnego archiwum **Harry Potter (PC 2002), bez miotły** w prywatnej pamięci, pobranych plikach i na stronie modelu. Gdy model jest już pobrany do Downloads, odnajduje go bez ręcznej ścieżki.
- Jeśli dostępny jest Blender, `blender_prepare.py` przygotowuje scenę i przetwarza wszystkie cztery tekstury PNG na bezpiecznie nazwane `PH_HARRY_TEX0..3.TGA` dla Gothica.
- `blender_visual_first.py` przygotowuje dopasowanie Harry'ego do szkieletu Gothica, usuwa oryginalne wagi kości HP2 i sprawdza, czy minimum 85% wierzchołków otrzymało wagi ludzkiego szkieletu. Wynikiem nadal jest **podgląd .blend**, nie finalny ASC.
- `tools/stage_harry_export.py` automatycznie wykrywa `PH_HARRY.ASC` wyeksportowany obok oryginalnego FBX, poprawia referencje do tekstur na `PH_HARRY_TEX0..3.TGA` i przenosi pliki do prywatnego folderu `HP2-Harry-converted`.
- `tools/visual_install.py` **włącza oryginalny wygląd dopiero po wykryciu lokalnie przekonwertowanego modelu** `Gothic/ParryHotter-private/HP2-Harry-converted/PH_HARRY.ASC` i czterech tekstur TGA. Sprawdza obecność **`*MESH_SOFTSKINVERTS` i kości `Bip01`**, żeby odmówić instalacji nieruchomego modelu w pozie T. Dopisuje model do `HUMANS.MDS`, tworzy strój, aktualizuje bohatera i przygotowuje źródła do ponownego parsowania.
- `ZBUDUJ_MOD_PO_REPARSE.bat` umieszcza prywatny model i tekstury **tylko w Twoim lokalnym .mod**, jeśli źródła i skompilowany DAT są kompletne. Publiczne repozytorium nie zawiera modelu z gry Harry Potter.
- `SPRAWDZ_PARRY_HOTTER.bat` rozpoznaje tymczasową szatę i sprawdza, czy prawdziwy model ASC oraz jego tekstury są aktywne.

**Nadal brakuje:** wygenerowania i sprawdzenia prawdziwego pliku ASC z oryginalnego FBX, importu szkieletu Gothica do Blendera, modelu różdżki i testu animacji peleryny w ZenGin. Gdy użytkownik wyeksportuje `PH_HARRY.ASC` obok źródłowego FBX, **obecny instalator zajmie się pozostałym lokalnym etapem automatycznie**. Nie deklarujemy, że HP2 Harry jest już widoczny w grze.

## Etap 1/5: kamera i celowanie (wersja developerska)

**Wprowadzono:** płynne przybliżenie kamery do ramienia przy PPM, powrót do pierwotnych parametrów po zwolnieniu przycisku, filtrowanie martwych i oddalonych NPC, raycast ze środka kamery (zasłony/ściany), wyłączenie interakcji podczas ekwipunku i cutscenek oraz poprawkę konfiguracji, która nie wymusza już globalnego `Gothic2_Control=1`. Dodano testy C++ kamery i nowe testy przywracania ustawień myszy. **Działanie w silniku Gothic 1 nadal niepotwierdzone.**

Następny etap: model Harry'ego z peleryną i różdżką — wymaga prawdziwego FBX i konwersji do zgodnego z Gothikiem modelu ASC.

## Sterowanie z Komnaty Tajemnic — HUD / Union v0.4 DEV (nowe)

Powstał **eksperymentalny plugin Gothic I Union** i osobne oryginalne tekstury do paska zaklęć. Szczegóły, status i ograniczenia: **[union/README.md](union/README.md)**.

- Sześć kwadratowych slotów zaklęć u dołu ekranu — cały czas podczas rozgrywki, klawisze **1–6**.
- Mała centralna kropka celownika.
- Świecąca ikona wybranego czaru pojawia się przy celu, gdy Gothic rozpoznaje żywego NPC w polu skupienia.
- **Trzymaj PPM**, żeby przygotować czar; **kliknij LPM podczas trzymania PPM**, żeby rzucić pojedynczy czar na zaznaczonego NPC.
- Usunięto znany konflikt: PPM normalnie powoduje skok w Gothic 1 — plugin zawiera eksperymentalny hook, żeby zapobiec skokowi podczas celowania.
- 14 nowych tekstur TGA generowanych automatycznie i osobno pakowanych do ParryHotter.mod.

**Ważne:** kompilacja DLL Gothic I przeszła i wydanie [hud-dev](https://github.com/Bonziuu/ParryHotter/releases/tag/hud-dev) istnieje. Testy C++/Python mogą przechodzić, lecz **nie potwierdzono jeszcze rzeczywistego rzucania czarów w uruchomionym Gothic 1**. Dodatkowo standardowa kamera Gothica nie jest idealną kopią kamery Harry Potter PC. Skrótów 1–6 może też dotyczyć konflikt ze standardowym wybieraniem broni; testy gry są konieczne.

### Możliwie proste lokalne uruchomienie

1. Pobierz najnowszy ZIP projektu i wykonaj **INSTALUJ_PARRY_HOTTER.bat**. Instalator pobierze polskie skrypty, zainstaluje czary i utworzy tekstury ikon.
2. W GothicStarter_mod wybierz **Parry Hotter (DEV)**, zaznacz **Reparse Scripts** i uruchom nową grę, żeby skompilować pliki; po kompilacji wyjdź.
3. W folderze projektu wykonaj **ZBUDUJ_MOD_PO_REPARSE.bat** — próbuje zbudować **ParryHotter.mod** zawierający skompilowane skrypty, polskie napisy, 14 ikon oraz podpisany sumą SHA-256 eksperymentalny plik DLL z GitHuba.
4. Uruchom profil **Parry Hotter (PLAY)** z GothicStarter. Graj tylko na nowych/testowych zapisach, ponieważ DLL nie został jeszcze zweryfikowany w samej grze.

Brakuje nadal fizycznej konwersji modelu Harry'ego w pelerynie do ASC oraz zaawansowanej fizyki NPC. Nie traktuj prototypu jako kompletnej gry.

## Archiwum v0.3 (nieaktualne: informacje poniżej nie są instrukcją dla v0.8)

- **Rictusempra** nie uruchamia już zwykłego wielominutowego szału po trafieniu człowieka przez gracza. Zamiast tego uruchamia krótki stan animacji `T_PSI_VICTIM` i `T_SEARCH` (zastępcze animacje stockowe).
- **Avada Kedavra** losuje jedną z dwóch dostępnych animacji przed obrażeniami śmiertelnymi. Nadal nie jest to ragdoll ani gotowy komediowy zestaw animacji.
- **Mysz:** instalator ustawia `Gothic2_Control=1` w `SystemPack.ini` i `enableMouse=1` w `Gothic.ini`, o ile odpowiednie klucze istnieją. Oryginalne pliki zapisuje jako `.parryhotter-original`. Wybrany czar można rzucać przez LPM. **Rolka i osobne celowanie PPM nadal nie istnieją**.
- **Osobny mod:** po poprawnej kompilacji `Gothic.dat` przez GothicStarter_mod uruchom `ZBUDUJ_MOD_PO_REPARSE.bat`. Wymaga narzędzia `GothicVDFS.exe` z ModKitu. Tworzy `Data/ModVDF/ParryHotter.mod` i `System/ParryHotter-PLAY.ini`. Profil PLAY nie ma `-zreparse`, więc korzysta z pakietu. Nadal nie zawiera skórki Harry'ego.
- **Testy:** GitHub Actions przeszły na Windows i Linux, przygotowanie prawdziwego polskiego zestawu Unified-PL oraz kompilację składni całego zestawu Daedalus niezależnym kompilatorem GDC. Na potrzeby wyłącznie tego testu GDC wymaga tymczasowej adaptacji klasy `C_NPC` oraz historycznej nazwy stałej. **Powstały w CI DAT nie jest kompatybilnym artefaktem Gothic 1 do instalowania**; nadal nie przeprowadzono testu oryginalnym parserem ZenGin ani w samej grze.

Nadal **brak gotowego skórkowanego modelu ASC, modelu różdżki, wtyczki fizyki NPC i prawdziwej integracji w silniku**. Nie traktuj wersji DEV jako gotowego wydania; nie zastępuje ona testów w Gothic 1.

## Stan prac: co jest gotowe, a co nie

**Gotowe do testu:** polskie skrypty/napisy, sześć run, wstępne animacje Rictusempra i Avada, bezpieczna kopia zapasowa, zmiana sterowania LPM w INI, automat do utworzenia własnego `.mod` z lokalnie skompilowanych plików.

**Nieukończone:** działający w Gothic 1 model `.ASC` Harry'ego z peleryną, własny efekt lewitacji i odrzutu w powietrze (ragdoll), zmiana zaklęć rolką, oddzielne celowanie PPM, potwierdzenie działania run w silniku gry. Skrypt Blendera przygotowuje tylko podgląd: **nie jest automatyczną konwersją modelu do gry**.

**Nie publikujemy moda jako „gotowego”** przed testem oryginalnym GothicStarter, pakowania GothicVDFS i rzeczywistym uruchomieniem.

## Instalacja możliwie najprostsza (Steam Windows)

Wymagania: Gothic 1 + Gothic ModKit (masz już plik `_work/DATA/scripts/Content/Gothic.src`), `System/GothicStarter_mod.exe` i Python 3 (`py -3` — w poprzednich testach działał).

1. Pobierz repozytorium: [Code → Download ZIP](https://github.com/Bonziuu/ParryHotter/archive/refs/heads/main.zip), rozpakuj.
2. **Kliknij dwa razy `INSTALUJ_PARRY_HOTTER.bat`.** Installer jest ustawiony pod Twoją ścieżkę: `C:\Programy\Steam\steamapps\common\Gothic`.
3. Program automatycznie pobierze polskie skrypty [Unified-PL](https://github.com/VaanaCZ/gothic-1-classic-scripts/tree/Unified-PL), przeniesie dotychczasowy folder `Content` do `Content_ParryHotter_Backup_...`, zainstaluje runy, bohatera i polski `OU.bin` oraz skopiuje `ParryHotter.ini`.
4. Otworzy się `GothicStarter_mod.exe`. Wybierz `Parry Hotter (DEV)`, **Reparse Scripts** i uruchom. Zacznij **nową grę**.
5. Rzuć czar standardowym sterowaniem Gothica: wyposaż runę, wybierz numer runy, celuj w postać i użyj kombinacji akcji + do przodu.

**Uwaga:** Skrypt pobiera dane wyłącznie z konkretnego, przypiętego commita Unified-PL. Wymaga połączenia z GitHubem. Nie pobiera niczego z podejrzanych serwisów z plikami EXE. Gdy pobieranie zawiedzie, zgłasza błąd; nie zmienia źródeł gry przed walidacją i utworzeniem kopii. Nie uruchamiaj instalatora przy włączonym Gothicu.

Uruchamianie z inną ścieżką (PowerShell w katalogu repo):
```powershell
py -3 tools/setup.py --game "D:\Gry\Gothic" --run
```

## Co jest w prototypie

| Runa | Aktualna baza Gothic 1 | Stan |
| --- | --- | --- |
| Flipendo | Fist of Wind | odrzut |
| Levicorpus | Ice Block | zamrożenie, bez lewitacji |
| Rictusempra | Berserk | szał, bez własnych animacji |
| Expelliarmus | Ball Lightning | ogłuszenie + próba upuszczenia broni |
| Bumbardo Maximus | Storm Fist | mocniejszy odrzut |
| Avada Kedavra | Breath of Death | próba porażenia i śmierci NPC |

Bohater ma imię Parry Hotter, 200 many, 6 krąg magii i overlay `Humans_Mage.mds`.

**Nie przetestowano jeszcze w prawdziwym silniku** mechanik w stanie końcowym. Testy GitHub Actions sprawdzają kod instalacyjny i zgodność struktury polskich plików, nie grę.

## Priorytet: wyglad Harry'ego i peleryna (Visual First)

**Nie kopiujemy hitboxow ani oryginalnych animacji z HP2.** Zachowujemy bryle modelu, twarz, wlosy, tekstury i szkolna peleryne, a do ruchu wykorzystujemy **zwykly szkielet czlowieka Gothic 1**.

Nowy skrypt `tools/blender_visual_first.py`:
- importuje niezmieniony model `skharrymesh.fbx` i jego materialy,
- usuwa obiekty z nazwa typu broom/Quidditch (jezeli takie wystepuja),
- skaluje model wzgledem ludzkiej postaci Gothica,
- przenosi wagi kosci z siatki Gothic na siatke Harry'ego, a odrebna peleryne przyczepia do plecow,
- zapisuje podgladowy projekt Blendera `ParryHotter_VISUAL_FIRST.blend`, bez zamiany fizyki ani hitboxow.

Aby uruchomic ten etap **potrzebna jest referencja szkieletu**: `Gothic/ParryHotter-private/GothicHuman.blend` z zaimportowanym przez KrxImpExp `HUM_BODY_NAKED0.ASC`. Jezeli Blender i referencja sa dostepne, instalator automatycznie przygotuje podglad. To nie jest jeszcze wynikowy plik `.ASC` — potrzebny jest eksport ASC zgodny z ZenGin, rejestracja siatki w MDS, aktywacja modelu i test w silniku.

Brak pliku z modelem lub poprawnego szkieletu nie moze zostac magicznie zastapiony przez sam skrypt Daedalus. Nie deklarujemy, ze model jest podmieniony, dopoki nie powstanie poprawny ASC i nie zostanie przetestowany w Gothic 1.

## Automatyczne przygotowanie oryginalnego modelu Harry'ego (bez miotly)

Instalator `INSTALUJ_PARRY_HOTTER.bat` **probuje dodatkowo pobrac** zwyklego Harry'ego z The Models Resource (dokladnie `295618.zip`, a nie wariant Quidditch). Wyodrebnia pliki do osobnego katalogu:

`Gothic/ParryHotter-private/HP2-Harry-original/`

NIE dodaje ich do GitHuba ani do katalogu danych Gothica. Jezeli na komputerze jest Blender, tworzy rowniez prywatny plik `Harry_HP2_to_Gothic_PREP.blend` z FBX i teksturami.

Gdy strona blokuje automatyczne pobieranie, pobierz ZIP recznie z [modelu Harry'ego](https://models.spriters-resource.com/pc_computer/harrypotterthechamberofsecrets/asset/295618/) i uruchom w PowerShellu:

```powershell
py -3 tools/model_asset.py --zip "C:\Users\TWOJA_NAZWA\Downloads\295618.zip"
```

**Wazne:** taki plik `.blend` to dopiero pierwszy etap. Model z HP2 ma inny szkielet niz Gothic. Aby bohater wygladal i animowal sie prawidlowo, potrzebny jest retarget do szkieletu Gothic 1, eksport skinned mesh do `.ASC` poprzez KrxImpExp, przygotowanie/kompilacja MDS oraz test uruchomienia. **Sam FBX/OBJ nie podmienia postaci w grze**. Do tej pory nie zostal wyeksportowany dzialajacy `.ASC`. Zrodlo modelu nie jest rozpowszechniane w repo.

## Model Harry'ego z Komnaty Tajemnic (PC, 2002) — **prywatny prototyp**

Użytkownik przygotowuje Parry Hotter **wyłącznie do osobistych, niekomercyjnych testów i nauki**, bez publikowania moda z modelem. Możemy więc zaplanować **lokalne użycie** oryginalnego modelu z HP2. [Regulamin The Models Resource](https://models.spriters-resource.com/page/tou/) dopuszcza wykorzystywanie zasobów w niepublikowanych, niekomercyjnych projektach w granicach obowiązującego prawa; nie oznacza to automatycznego przeniesienia praw autorskich.

**Dokładny model:** [Harry Potter — PC Chamber of Secrets (The Models Resource)](https://models.spriters-resource.com/pc_computer/harrypotterthechamberofsecrets/asset/295618/), przesłany przez IndigoPupper. Archiwum zawiera `skharrymesh/skharrymesh.fbx`, wersję `.obj`, materiały i tekstury `skharryTex0.png`–`skharryTex3.png`. **FBX** jest lepszym materiałem wyjściowym do szkieletu/animacji niż OBJ.

**Plan lokalnego importu** (nadal do wykonania):
1. Pobierz archiwum na swój komputer i pozostaw je poza publicznym repozytorium, np. w prywatnym `ParryHotter-local/models/`.
2. Zaimportuj `skharrymesh.fbx` do Blendera i porównaj jego szkielet z `HUM_BODY_NAKED0.ASC` z Gothic 1.
3. Dopasuj proporcje i wiązanie kości do szkieletu Gothic 1, następnie wyeksportuj model przez KrxImpExp do formatu `.ASC`. Animacje rąk oraz rzucania czarów będą wymagały dodatkowych testów.
4. Skonfiguruj model w MDS/skryptach Gothica, sprawdź tekstury i przetestuj w GothicStarter.

**Stan obecny:** znaleziono źródło i formaty modelu; model **nie jest jeszcze pobrany, skonwertowany ani działający w Gothic 1**. Instalator v0.2 nie obiecuje podmiany postaci. Nie publikujemy cudzych plików na GitHubie; lokalny workflow może używać osobno pobranej kopii.

## Stan prac

Zobacz [ROADMAP.md](ROADMAP.md), a problemy i diagnostykę w [docs/PL-PROBLEMY.md](docs/PL-PROBLEMY.md).

To fanowski, niekomercyjny projekt, niezwiązany z właścicielami gier Gothic i Harry Potter.
