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

## Co z oryginalnym modelem Harry'ego z gry PC 2002?

Model rzeczywiście jest prezentowany w serwisie [The Models Resource — HP2 PC](https://models.spriters-resource.com/pc_computer/harrypotterthechamberofsecrets/), ale publikacja wyciągniętego modelu na stronie nie oznacza, że otrzymaliśmy zgodę właścicieli praw na umieszczenie go w publicznym modzie. **Nie dołączamy go** i nie udajemy, że potrafi sam się zainstalować: szkielet i animacje UE1 trzeba dopasować do ZenGin.

Na potrzeby wersji publicznej można wykonać autorski model Parry'ego albo użyć legalnie dostępnej postaci **CC0** (np. [Low Poly RPG Characters](https://opengameart.org/content/lowpoly-rpg-characters)) i skonwertować ją do szkieletu Gothica. Własny eksport z posiadanej gry Harry Potter może być używany lokalnie w zależności od warunków/licencji.

## Stan prac

Zobacz [ROADMAP.md](ROADMAP.md), a problemy i diagnostykę w [docs/PL-PROBLEMY.md](docs/PL-PROBLEMY.md).

To fanowski, niekomercyjny projekt, niezwiązany z właścicielami gier Gothic i Harry Potter.
