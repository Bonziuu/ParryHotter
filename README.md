# Parry Hotter — Gothic 1 🪄

**Wersja 0.3 DEV — polskie skrypty, sześć run, sterowanie LPM i narzędzie do budowania osobnego .mod.**

To nadal **prototyp źródłowy**, a nie kompletny mod `.mod`. Nie ma jeszcze modelu Harry'ego z `Komnaty Tajemnic`, fizyki ragdoll ani własnego sterowania klikaniem myszką. Nie gwarantujemy działania wszystkich efektów — instalacja i parsowanie Gothic 1 na komputerze gracza nadal wymagają testów.

## Co zmieniono w v0.3 (przed kolejnym testem)

- **Rictusempra** nie uruchamia już zwykłego wielominutowego szału po trafieniu człowieka przez gracza. Zamiast tego uruchamia krótki stan animacji `T_PSI_VICTIM` i `T_SEARCH` (zastępcze animacje stockowe).
- **Avada Kedavra** losuje jedną z dwóch dostępnych animacji przed obrażeniami śmiertelnymi. Nadal nie jest to ragdoll ani gotowy komediowy zestaw animacji.
- **Mysz:** instalator ustawia `Gothic2_Control=1` w `SystemPack.ini` i `enableMouse=1` w `Gothic.ini`, o ile odpowiednie klucze istnieją. Oryginalne pliki zapisuje jako `.parryhotter-original`. Wybrany czar można rzucać przez LPM. **Rolka i osobne celowanie PPM nadal nie istnieją**.
- **Osobny mod:** po poprawnej kompilacji `Gothic.dat` przez GothicStarter_mod uruchom `ZBUDUJ_MOD_PO_REPARSE.bat`. Wymaga narzędzia `GothicVDFS.exe` z ModKitu. Tworzy `Data/ModVDF/ParryHotter.mod` i `System/ParryHotter-PLAY.ini`. Profil PLAY nie ma `-zreparse`, więc korzysta z pakietu. Nadal nie zawiera skórki Harry'ego.
- **Testy:** GitHub Actions sprawdza instalację na Windows/Linux, prawdziwy polski zestaw Unified-PL, skrypty Pythona i kompilację Daedalus przez niezależny kompilator (wynik tego ostatniego testu należy sprawdzić w Actions).

Nadal **brak gotowego skórkowanego modelu ASC, modelu różdżki, wtyczki fizyki NPC i prawdziwej integracji w silniku**. Nie traktuj wersji DEV jako gotowego wydania; nie zastępuje ona testów w Gothic 1.

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
