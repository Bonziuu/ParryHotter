Kontynuuj rozwój projektu Parry Hotter do grywalnego, sprawdzonego moda Gothic I Classic na PC. Przenoszę projekt z innego czatu. W tym nowym czacie środowisko i dostęp do narzędzi mają być już przygotowane — najpierw sprawdź ich rzeczywisty stan i wykorzystaj wykonaną pracę.

1. AKTUALNE ŹRÓDŁA I PLIKI

Repozytorium: Bonziuu/ParryHotter. Aktualna gałąź przekazania: codex/dev013-handoff. Etap: DEV 0.13. Pobierz tę gałąź i przeczytaj niniejszy plik PROMPT_DO_NOWEGO_CZATU.md.
Aktualny kod, ten prompt oraz przygotowana paczka ParryHotter_Harry_Gothic.zip są opublikowane na gałęzi codex/dev013-handoff. ZIP znajduje się w katalogu głównym repozytorium i zawiera rzeczywisty model, cztery tekstury, dwa klipy ASC, nakładkę MDS, projekty Blendera i raporty. Zwykłe pobranie gałęzi przenosi również te zasoby; Git LFS nie jest wymagany. Instrukcja dostępu, kontroli sumy i importu jest w docs/MODEL_DLA_CLAUDE_CODE.md. Instalator wykrywa paczkę automatycznie, więc nie trzeba prosić użytkownika o ponowny upload ani wykonywać konwersji od zera. Pełny starszy snapshot ParryHotter_DEV013_sources_and_model.zip pozostaje dodatkowo w poprzednim środowisku.

W poprzednim czacie powstało wiele zmian i nowych plików. Są opublikowane na gałęzi codex/dev013-handoff; późniejsze poprawki i potwierdzenie kompilacji DLL także znajdują się na tej gałęzi. Gałąź main może nadal zawierać starszy prototyp. Najpierw sprawdź git status, wybierz właściwą gałąź, porównaj źródła ze stanem nowego czatu i zachowaj istniejące zmiany oraz przygotowane środowisko.

SHA-256 starszego archiwalnego snapshotu źródeł (aktualny kod pobieraj z gałęzi):
8a8f2ad83e255fdcaa24a9338273baa7ee679425ba409be0c56676bf34c463c0
SHA-256 osadzonej paczki modelu:
f0378e38ee1935ac7b7f420c592e00d464e7cfc7d91db6c7124bdd14b65b72f9

Dokumentem bieżącej wersji jest docs/DEV-013.md, następnie ROADMAP.md i początek README.md. Dalsze rozdziały README.md oraz union/README.md zawierają historię starszych wersji i niektóre nieaktualne ograniczenia. Ustal aktualną prawdę na podstawie kodu i dokumentacji DEV 0.13.

Użytkownik następnie wyraźnie zezwolił na publiczną publikację przygotowanej paczki modelu na GitHubie, aby Claude Code mógł pobrać i scalić komplet projektu. To aktualizuje wcześniejsze ustalenie o pozostawieniu tej paczki poza repozytorium. Użyj opublikowanego ZIP-a z zachowanym pochodzeniem w MANIFEST.json i INFOmesh_original.txt.

2. DOCELOWE DZIAŁANIE MODA — UZGODNIONE WYMAGANIA

A. Główny bohater:
- Zastąp wygląd głównego bohatera rzeczywistym Harrym Potterem z Harry Potter i Komnata Tajemnic, WERSJA PC.
- Zachowaj jego twarz, włosy, okulary z przezroczystością, szatę, pelerynę, UV i tekstury.
- Model ma poprawnie animować się na szkielecie Gothic I, bez pozy T, skręconych kończyn, zdeformowanej głowy i podwójnej głowy Gothica.
- Zmiana zbroi ma zachowywać wygląd Harry’ego oraz normalne statystyki wyposażenia.
- Zmiana dotyczy bohatera; wygląd pozostałych NPC pozostaje prawidłowy.
- Widoczna różdżka musi podążać za dłonią i animacją. Sprawdź jej położenie, chwyt i gniazdo ZS_RIGHTHAND.

B. HUD:
- Podczas zwykłej rozgrywki stale pokazuj DZIESIĘĆ kwadratowych pól, opisanych 1,2,3,4,5,6,7,8,9,0.
- Pola 1–6 odpowiadają istniejącym sześciu zaklęciom.
- Moja wyraźna decyzja: pola 7,8,9,0 POZOSTAJĄ PUSTE do późniejszego wyboru. Nie dodawaj tam automatycznie kolejnych czarów.
- Wybranie pustego pola nie przygotowuje czaru, nie rzuca go i nie zużywa many.
- Zachowaj kwadratowy kształt ikon w pikselach przy różnych rozdzielczościach i proporcjach ekranu.
- HUD i znaki mają być rysowane wewnątrz gry.

C. Kamera i ruch:
- Swobodna kamera trzecioosobowa inspirowana Harry Potter 1 i 2 na PC, obracana myszą w poziomie i pionie.
- Bardzo szybkie, swobodne poruszanie w każdym kierunku względem kamery: WASD/strzałki, również po skosie, bez zwiększania prędkości na skosach.
- Łatwe skakanie Spacją, sprawna zmiana kierunku podczas skoku i dobre sterowanie w powietrzu.
- Zachowaj kolizje, podłoże, grawitację oraz przewidywalne zachowanie przy ścianach, sufitach, schodach i krawędziach.
- Obecne wartości startowe: bieg 650 cm/s, celowanie 450 cm/s, przyspieszenie na ziemi 4200, hamowanie 5000, przyspieszenie w powietrzu 2600, hamowanie w powietrzu 120, bufor skoku 150 ms, coyote time 120 ms. Dostrój odczucie w grze.
- Natywne CanJump może odrzucać próbę skoku pomimo coyote time w przenośnym kontrolerze; trzeba to rzeczywiście sprawdzić i dopracować.
- F11 przełącza nowy ruch/kamerę i ruch natywny. Wybieranie czarów nadal działa bez konfliktów z natywnym wyborem broni.

D. Czarowanie — dokładna kolejność:
- Najpierw TRZYMAM PRAWY PRZYCISK MYSZY. Bohater przygotowuje różdżkę i wykonuje animację czarowania.
- Gdy namierzę odpowiedniego żywego NPC właściwym czarem, przy NPC pojawia się symbol wybranego zaklęcia.
- Symbol ma przypominać znaki z HP PC: świecący, lekko falujący/uniesiony, pulsujący i delikatnie animowany w powietrzu.
- Powinien być związany z pozycją NPC i rzutowany na ekran, a nie stale przyklejony do środka celownika.
- Dopiero NOWE kliknięcie LEWEGO PRZYCISKU przy nadal trzymanym PPM rzuca czar na wskazanego NPC.
- Jednoczesne wciśnięcie PPM i LPM oraz wcześniejsze przytrzymywanie LPM nie mogą powodować przypadkowego rzutu. Trzymanie LPM nie powtarza rzutu.
- Puszczenie PPM lub utrata celu ukrywa znak zgodnie ze stanem celowania.
- Promień ze środka kamery ma respektować ściany i przeszkody. Martwy NPC, zbyt odległy cel i cel niepoprawny dla danego czaru są odrzucane. Obecny zasięg wynosi około 19 m.
- Koszt many, przygotowanie, odnowienie i trafienie muszą być zgodne z rzeczywistym wynikiem silnika. Nieudany rzut nie może rozpoczynać cooldownu.
- Kilka lecących pocisków i zmiana wybranego czaru nie mogą pomylić ofiary ani efektu wcześniejszego pocisku.
- Pauza, ekwipunek, rozmowa, scenka, zapis/wczytywanie, zmiana świata, śmierć, utrata przytomności, pływanie, wspinanie i sterowanie przez skrypt muszą prawidłowo przerwać/przekazać sterowanie.
- Po przerwaniu należy zwolnić PPM przed ponownym przygotowaniem; dotyczy to też zmiany opcji i przebudowy HUD.

3. SZEŚĆ CZARÓW I POZOSTAŁE BRAKI ICH EFEKTÓW

Kolejność zachowujemy:
1 — Flipendo.
2 — Levicorpus.
3 — Rictusempra.
4 — Expelliarmus.
5 — Bumbardo Maximus.
6 — Avada Kedavra.
7,8,9,0 — puste.

Obecne podstawy w Gothicu: odpowiednio Windfist, IceCube, Berzerk, Thunderball, Stormfist, BreathOfDeath. To podstawa prototypu, nie dowód ukończenia docelowych efektów.

Do dopracowania i sprawdzenia:
- Flipendo: czytelny, rzeczywisty odrzut trafionego celu.
- Levicorpus: rzeczywiste uniesienie i zawieszenie NPC głową w dół, następnie bezpieczny powrót. Obecnie jest około 4-sekundowy stan zastępczy i ograniczony impuls; brak ukończonej odwróconej lewitacji.
- Rictusempra: właściwa krótka, komiczna reakcja/animacja. Obecnie używane są zastępcze animacje Gothica.
- Expelliarmus: ogłuszenie i rzeczywiste rozbrojenie po trafieniu; sprawdzić upuszczanie trzymanej broni.
- Bumbardo Maximus: wybuch, wyrzut NPC, obrót/reakcja fizyczna, kolizje i lądowanie. Obecny ograniczony impuls i animacja lądowania nie stanowią pełnego ragdolla.
- Avada Kedavra: dopracować efekt oraz zaplanowaną reakcję/śmierć. Obecnie bazowy BreathOfDeath może zabić słabego NPC przed animacją, a skrypt używa zastępczych reakcji.
- Zbadaj i dopracuj pełny ragdoll/obroty oraz rzeczywistą fizykę NPC na możliwościach Gothic I/Union. Aktualny impuls wymaga już aktywnego zCRigidBody/physicsEnabled i może być pomijany dla wielu NPC.
- Nie rozwiązuj tego przez globalną zmianę obrażeń bazowych zaklęć wszystkich NPC. Zachowaj poprawne działanie pozostałej gry.
- Po zakończeniu reakcji, śmierci lub przerwaniu NPC nie może zostać trwale zawieszony, unieruchomiony ani pozbawiony rutyny AI.

4. CO JUŻ ZROBIONO W KODZIE

- Dziesięć pól, sześć czarów, cztery puste; 15 tekstur HUD.
- Kontroler PPM -> świeży LPM, widoczność i animacja znaku celu.
- Ruch względem kamery, swobodny obrót, sterowanie skokiem, bufor i coyote time.
- Adapter natywnego ruchu z kolizjami; usunięto podwójne przesuwanie poziome i konflikt komendy biegu bezpośrednio po rozpoczęciu skoku.
- Wspólne blokady bezpiecznego stanu dla ruchu/czarów; poprawiono własność klawiszy i akcji myszy także po F11.
- Potwierdzanie oCSpell::Cast oraz obsługa odroczonego wywołania animacji; cooldown po potwierdzeniu sukcesu.
- Rejestr pocisków powiązanych z rzeczywistym FX, ofiarą i zaklęciem, ograniczony czas życia oraz pojedyncze zastosowanie impulsu.
- Ochrona referencji i generacji żądania przed zmianą stanu podczas natywnych callbacków.
- Konwersja rzeczywistego Harry’ego, kostium, usuwanie starej głowy i kod zachowania wyglądu po zmianie pancerza.
- Własne animacje różdżki oraz opcjonalne powiązanie sześciu uchwytów animacji wyłącznie u gracza.
- Instalator i importer z walidacją przed zapisem, kopiami zapasowymi i transakcyjnym wycofaniem zmian.
- Pakowanie .mod/profilu PLAY z przywracaniem poprzednich plików po błędzie.
- Odrzucanie starej DLL i nieaktualnego Gothic.dat.
- F10: diagnostyka celowania, trafień, impulsów, lądowań i powodów pominięcia fizyki.

Ważniejsze pliki:
union/Plugin/plugin.cpp
union/Plugin/PlayerControls.hpp
union/Plugin/CastingAnimations.hpp
union/hud/SpellHudState.hpp
union/hud/FreeCamera.hpp
union/hud/Locomotion.hpp
union/hud/TargetGlyph.hpp
union/hud/CastLedger.hpp
mod/Magic/ParryHotter.d
tools/setup.py
tools/import_harry_bundle.py
tools/visual_install.py
tools/casting_assets.py
tools/file_transaction.py
tools/blender_visual_first.py
tools/blender_casting.py
tools/blender_krx.py
tools/build_union.ps1
tools/package_mod.py
tools/spell_catalog.py
tools/check.py
tools/verify_daedalus.py
docs/DEV-013.md

5. RZECZYWISTY MODEL I ANIMACJE — TEGO NIE ZACZYNAJ OD ZERA

Użytkownik dostarczył archiwum:
PC _ Computer - Harry Potter and the Chamber of Secrets - Characters - Harry Potter.zip

Stary FBX 6000 nie działał z importerem Blendera, więc wykorzystano dołączone OBJ/MTL, odzyskano oryginalne wiązania/wagi z FBX i wykonano retarget na prawdziwy szkielet Gothica. Zachowano dwustronne trójkąty szaty oraz alfa okularów.

Aktualny wynik:
- 25 kości Gothica, 710 ważonych wierzchołków, 1016 trójkątów.
- 35 wierzchołków peleryny i 249 wierzchołków związanych z głową.
- Model, twarz, włosy i okulary przeszły próby obrotu głowy, ręki i kolana.
- Poprawiono numeryczną utratę precyzji starego eksportera przy obrotach bliskich 180 stopni.
- Ponowny import rzeczywistego modelu ASC zachował geometrię z błędem poniżej 0,000000154 m.

Osiem plików gotowego zestawu źródłowego:
PH_HARRY.ASC
PH_HARRY_TEX0.TGA
PH_HARRY_TEX1.TGA
PH_HARRY_TEX2.TGA
PH_HARRY_TEX3.TGA
PH_HARRY_CAST.MDS
PH_HARRY_CHARGE.ASC
PH_HARRY_RELEASE.ASC

Dwa własne klipy czarowania: przygotowanie/pętla 43 klatki i rzut/powrót 26 klatek, 30 FPS. Przeszły eksport oraz ponowny import Kerrax; maksymalny błąd pozycji kości około 0,00000136 m. W paczce jest ParryHotter_CASTING.blend i podglądy.

Są to autorskie klipy inspirowane HP. Nogi i globalny korzeń pozostają nieruchome w tych klipach. Nadal trzeba dopracować chód nóg podczas celowania/ruchu oraz pełny zestaw animacji poruszania się, skakania i reakcji inspirowanych HP PC. Peleryna jest związana z kośćmi tułowia; pełna symulacja jej ruchu nie została ukończona.

Nakładka PH_HARRY_CAST.MDS ma sześć unikalnych animacji:
T_PH_PREPARE, S_PH_CHARGE, T_PH_RELEASE, T_PH_CANCEL, S_PH_RELEASE, T_PH_RECOVER.
Natywne TransitionToInvest/Cast pozostają właścicielem odtwarzania; adapter podmienia komplet uchwytów dopiero po odnalezieniu całego zestawu i przywraca własne uchwyty przy przerwaniu.

Instalator dodaje nakładkę tylko w konstruktorze PC_Hero, po Humans_Mage. Wymagana jest NOWA GRA do wykonania konstruktora. Stary zapis nie dostaje nakładki automatycznie. To trzeba uwzględnić w obsłudze zapisów.

Model bez trzech plików animacji nadal jest obsługiwany; opcjonalne animacje muszą być kompletne. Aktualizacja modelu nie może mieszać klipów z innym szkieletem/macierzą wiązania.

6. WYNIKI WERYFIKACJI W POPRZEDNIM CZACIE

Przeszły:
- python3 -m tools.check: 160 testów Python, 14 przenośnych programów C++17 i walidacja 15 tekstur.
- python3 -m tools.verify_download: prawdziwe źródła Unified-PL.
- python3 -m tools.verify_daedalus: baza i wariant stroju z nakładką animacji. Kontrolne DAT miały odpowiednio 3608048 i 3608524 bajty.
- Rzeczywisty eksport/import modelu i obu klipów w Blenderze 4.3.2/Kerrax.
- Instalacja na prawdziwych źródłach Unified-PL i HUMANS.MDS, ponowienie bez zmian oraz wycofanie po wymuszonym błędzie zapisu.
- Aktualizacja starszego zestawu pięciu plików do nowego zestawu ośmiu plików z zachowaniem kopii starego modelu.
- Rozpakowana końcowa paczka źródeł została użyta do rzeczywistego testu instalacji.
- Błąd eksportu drugiego klipu zachowuje poprzedni model, tekstury, animacje i projekt.

Istotne ograniczenie: testy C++ obejmują symulacje adapterów. Nie potwierdzają ABI Union ani działania ZenGin. GDC wymagał tymczasowej adaptacji C_NPC do niezależnego kompilatora; jego DAT służy wyłącznie kontroli kodu i NIE NADAJE SIĘ DO INSTALACJI W GOTHICU.

Po pierwotnym przekazaniu projektu kolejne commity na tej gałęzi potwierdziły kompilację aktualnej DLL przez MSVC w GitHub Actions: pierwszy udany build 6df5bca, PE32 i386, 180736 bajtów, SHA-256 55dc462d34d5afad4d64aeb17b1e0421b5c44c43942da3f29838ce7889ba9933. Aktualne instrukcje pobrania artefaktu i weryfikacji są w docs/DEV-013.md oraz tools/verify_dll.py. Raporty w ZIP opisują wcześniejszy moment konwersji i mogą nadal podawać native_dll_built=false. Nie uruchomiono jeszcze moda w Gothic I. Nie potwierdzono końcowej kompilacji modeli/tekstur/MDS/MAN przez silnik ani rzeczywistej końcowej budowy .mod. GIF jest renderem z Blendera.

7. KOLEJNOŚĆ DALSZEJ REALIZACJI

1) Pobierz aktualną gałąź codex/dev013-handoff wraz z dołączoną paczką modelu i sprawdź nowe środowisko. Zweryfikuj, co już przygotowano w tym nowym czacie. Zachowaj istniejące narzędzia, pliki i poprawki.

2) Wykorzystaj potwierdzony build aktualnej natywnej DLL z GitHub Actions albo narzędzia przygotowane w tym czacie. Sprawdź PE32 i386 i marker przez tools/verify_dll.py. Gdy zmienisz natywny kod lub brakuje aktualnego artefaktu, zbuduj ponownie Gothic I Classic, G1 Release / Win32, C++17, odpowiednie ABI i napraw błędy kompilacji/linkowania.
Windows:
powershell -ExecutionPolicy Bypass -File tools/build_union.ps1
Wymagany SDK UnionProject: commit 4ae1fc51bd962e72ed2a0b1d3da92084517db996.
Wynik: .parryhotter/build/ParryHotterHUD-G1.dll.
Kopiuj wszystkie potrzebne .hpp i rozszerzenia .inl. Skrypt zawiera poprawkę cytowania ścieżek TEMP z odstępami.

OBOWIĄZKOWY IDENTYFIKATOR NOWEJ DLL:
PH_BUILD_013_TEN_SLOTS_PROJECTILE_INPUT_CAST_ANIM

Sprawdź architekturę, identyfikator i rzeczywisty wynik budowania. Stara DLL z hud-dev i wcześniejsze DLL bez tej końcówki nie zawierają bieżących zmian.

3) Zainstaluj skrypty, model, różdżkę, HUD i animacje na właściwej instalacji Gothic I z ModKitem/Union:
py -3 tools/setup.py --game "SCIEZKA_DO_GOTHIC"
Dołączona paczka modelu obok BAT jest wykrywana automatycznie.
Alternatywny import:
py -3 -m tools.import_harry_bundle --game "SCIEZKA_DO_GOTHIC" --bundle "ParryHotter_Harry_Gothic.zip"

4) W GothicStarter_mod wybierz profil DEV, wykonaj Reparse Scripts i rozpocznij nową grę. Skompiluj potrzebne modele, tekstury, MDS oraz klipy do formatów silnika. Sprawdź odświeżenie cache. Same surowe ASC/MDS w archiwum nie dowodzą gotowości.

5) Doprowadź rzeczywiste zachowanie kamery, ruchu, skoku, różdżki, wszystkich sześciu czarów i animowanego znaku do opisanych wymagań. Dokończ brakujące efekty, lewitację głową w dół, reakcje i fizykę/ragdoll, z kontrolą kolizji oraz poprawnym przywracaniem AI.

6) Dopracuj deformacje modelu, chwyt różdżki, pelerynę oraz płynne połączenie animacji. Sprawdź ruch/skok podczas przygotowania czaru; obecne klipy nie animują chodu nóg.

7) Zbuduj prawdziwy lokalny .mod przy użyciu GothicVDFS i lokalnie skompilowanych danych:
ZBUDUJ_MOD_PO_REPARSE.bat
lub python -m tools.package_mod --game "SCIEZKA_DO_GOTHIC"
Używaj nowej lokalnej DLL. Sprawdź zawartość archiwum i profil PLAY. Wtyczka jest przeznaczona do wirtualnego System/Autorun wewnątrz moda.

8) Wykonaj testy w rzeczywistym Gothic I. Napraw regresje i ponów odpowiednie kontrole. Zaktualizuj dokumentację do rzeczywistego końcowego stanu.

9) Przygotuj grywalny pakiet oraz prostą polską instrukcję instalacji, uruchomienia, sterowania i przywracania poprzednich plików. Wyraźnie wymień ewentualne faktyczne blokady i pozostałe niedokończone elementy.

8. WYMAGANE PRÓBY W SILNIKU

- Poprawne ładowanie DLL, modelu, materiałów, tekstur, nakładki i różdżki.
- Dziesięć pól HUD przy różnych proporcjach ekranu; cztery puste pola pozostają puste.
- PPM uruchamia przygotowanie, osobny nowy LPM rzuca raz. Brak dodatkowego skoku, ataku, interakcji lub wyboru broni.
- Znak właściwego czaru podąża za poruszającym się NPC, faluje i znika po utracie prawidłowego celu.
- Celowanie przez ścianę, martwy NPC, zasięg i niewłaściwy cel nie pozwalają na nieprawidłowy rzut.
- Prawidłowa mana i cooldown, również przy nieudanym rzucie i opóźnionej animacji.
- Kilka pocisków, zmiana slotu i celu w czasie lotu, wiele callbacków obrażeń: właściwa ofiara i pojedynczy efekt.
- Szybki ruch w osiem stron, skoki, kierowanie w powietrzu, podłoże, sufity, ściany, schody i krawędzie.
- Powrót sterowania po rozmowie, ekwipunku, pływaniu, wspinaniu, obrażeniach, scenkach i śmierci.
- Pauza, zapis/wczytywanie, opcje/rozdzielczość oraz zmiana świata bez przypadkowego rzutu, awarii, wycieku referencji i pozostawionych uchwytów animacji.
- F11 zachowuje prawidłowy wybór czarów i blokowanie natywnych konfliktów.
- Harry zachowuje wygląd po zmianie zbroi i zapisach, nie ma drugiej głowy ani skręconych dłoni/stóp.
- Każdy czar osobno na odpowiednich celach; reakcja/lądowanie nie pozostawia NPC w uszkodzonym stanie AI.

9. ŚRODOWISKO I POPRZEDNIA BLOKADA

Poprzednia instancja miała działające Python/g++/Blender, ale pobranie Microsoft CRT/C++/Windows SDK było blokowane przez proxy:
aka.ms/vs/17/release/channel przekierowywało do download.visualstudio.microsoft.com, gdzie był CONNECT 403.

Domena była wpisana do szkicu ParryHotter, lecz stara maszyna nadal raportowała aktywną specyfikację 1 bez tego wpisu. Samo ponawianie zapisu/restartu nie odblokowało poprzedniej instancji. To opis starego czatu; w nowym środowisku dostęp może już działać. Sprawdź rzeczywisty stan i świeże połączenie.

Jeśli dostępne, wykorzystaj cloud-environment runtime i cloud-environment-onboarding:setup. Zachowaj proxy, weryfikację TLS, sumy i podpisy. Gdy narzędzia już są zainstalowane, użyj ich do budowania.

W poprzednim środowisku:
- /workspace/ParryHotter — źródła.
- /workspace/.parryhotter-cloud/native-toolchain — zweryfikowane LLVM 19.1.7, clang-cl/lld-link i xwin 0.10.0; był tylko testowy freestanding PE32 ABI smoke, NIE DLL ParryHotter.
- resume_microsoft_sdk.sh w tym katalogu wznawia xwin --arch x86 --variant desktop splat --use-winsysroot-style po uzyskaniu dostępu.
- /workspace/.parryhotter-cloud/UnionProject — przypięty SDK.
- /workspace/.parryhotter-cloud/krx-toolchain i /workspace/.parryhotter-cloud/blender-scripts — rzeczywisty importer/eksporter Kerrax.
- /workspace/.parryhotter-cloud/casting-animation — aktualne osiem plików, projekty i raporty.
- /workspace/.parryhotter-cloud/gothic-reference — rzeczywiste źródła referencji.
Te ścieżki mogą nie istnieć w nowym czacie. Załączona paczka zawiera aktualny kod, model, tekstury, projekty i raporty, ale nie cały zewnętrzny toolchain ani pliki zainstalowanej gry.

Referencja Gothica: PhoenixTales/gothic-devkit, commit 0b81c1932d11fd212c877cb8fd4e4a85ab2584de.
Poprzedni Kerrax: XarPL/KrxImpExp-Reworked 26d60ef2f9c0dacafc88a0af969b2e4dbbd16ad2 oraz houkama/KrxImpExp-Blender 5790ed047fe543987716e6c329cb6b2488dd62a0.
Ponowne autorstwo klipów jest dostępne przez tools/blender_casting.py z zainstalowanym eksporterem; gotowa paczka nie wymaga Blendera do samego importu do źródeł gry.

10. SPOSÓB PRACY I WARUNEK ZAKOŃCZENIA

Pracuj samodzielnie w ramach tych ustaleń, wykorzystuj równoległe zadania, jeśli są dostępne. Oczekuję wykonania i sprawdzania kolejnych etapów, a nie samego kolejnego spisu braków. Informuj po polsku o istotnych postępach. Pytaj tylko o informacje lub działania, które rzeczywiście blokują dalszą pracę.

Zachowaj istniejące pliki i kopie zapasowe. Waliduj zasoby przed zmianą gry. W przypadku błędu przywracaj poprzedni spójny zestaw i zachowuj dane do odzyskania.

Oddzielaj: kod zaimplementowany, kompilacja natywna, walidacja plików, kontrolny GDC i test faktycznej rozgrywki. Test syntetycznej siatki, sama obecność pliku MZ, stara DLL, zapisany workflow albo render Blendera nie potwierdzają ukończenia moda.

Najbliższy priorytet: pobrać opublikowaną paczkę modelu, wykorzystać zweryfikowaną aktualną DLL (lub przebudować po zmianie kodu), następnie skompilować zasoby i wykonać test w Gothic I. Dalej dokończyć wszystkie wymienione efekty, fizykę i animacje oraz przygotować sprawdzony pakiet grywalny. Jeśli pojawi się rzeczywista blokada, wykonaj pozostałą niezależną pracę i opisz dokładnie, czego brakuje do dalszego kroku.
