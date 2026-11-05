# Parry Hotter — Gothic 1 🪄

**Wersja 0.2 DEV — automatyczny instalator polskich skryptów i prototypu sześciu zaklęć.**

To nadal **prototyp źródłowy**, a nie kompletny mod `.mod`. Nie ma jeszcze modelu Harry'ego z `Komnaty Tajemnic`, fizyki ragdoll ani własnego sterowania klikaniem myszką. Nie gwarantujemy działania wszystkich efektów — instalacja i parsowanie Gothic 1 na komputerze gracza nadal wymagają testów.

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
