# Parry Hotter — polskie napisy i pierwsze testy

W Gothic 1 po użyciu opcji **reparse scripts** gra ponownie kompiluje pliki Daedalusa z lokalnego ModKitu. Jeżeli ModKit jest niemiecki, przetłumaczony wcześniej Gothic może mieć **polski dubbing, ale niemieckie napisy**. Odtwarzanie dubbingu i napisy pochodzą z innych zestawów danych.

## Zmiana źródeł na polskie

**Zrób kopię gry i wyłącz Gothica przed rozpoczęciem.**

1. Pobierz archiwum **Unified-PL** z projektu [VaanaCZ/gothic-1-classic-scripts](https://github.com/VaanaCZ/gothic-1-classic-scripts/tree/Unified-PL) (Code -> Download ZIP, zwróć uwagę na gałąź **Unified-PL**).
2. W swojej instalacji otwórz folder **Gothic/_work/DATA/scripts/**. Zmień nazwę obecnego folderu **Content** na **Content_DE_Backup**. **Nie usuwaj go.**
3. W pobranym archiwum znajdź **_work/Data/Scripts/Content**. Skopiuj ten folder **Content** do swojego **Gothic/_work/DATA/scripts/** (obok folderu Content_DE_Backup). To ważne: **nie mieszaj** polskich i niemieckich plików źródłowych w jednym folderze.
4. Pobierz **najnowszą** wersję ParryHotter z tego repozytorium (stara kopia z pulpitu może mieć starszy instalator).
5. Otwórz PowerShell w folderze pobranego ParryHotter-main i wpisz:

       py .\tools\install.py --content "C:\Programy\Steam\steamapps\common\Gothic\_work\DATA\scripts\Content" --dry-run

   Oczekuj listy **Would change** bez błędów. Następnie wykonaj tę samą komendę **bez** `--dry-run`. Zostaną ponownie dodane nasze 6 run i ustawienia gracza do **polskich** źródeł.
6. Uruchom **GothicStarter_mod.exe**, wybierz **Parry Hotter (DEV)**, zaznacz **Reparse all scripts**, uruchom i wybierz **Nowa gra**.

Jeśli nadal napisy są niemieckie, najpierw upewnij się, że w folderze **Content** jest polska wersja skryptów (np. w `Story/NPC/PC_Hero.d` przed patchem było `name = "Ja";`), oraz że zaznaczono reparse. Nie podmieniaj skompilowanych plików *.DAT na ślepo: mogą nadpisać mod. Menu/napisy niezwiązane z dialogami mogą zależeć też od oddzielnych plików menu i fontów.

Repozytorium ze źródłami: https://github.com/VaanaCZ/gothic-1-classic-scripts/tree/Unified-PL . Skrypty z tego projektu należy pobrać we własnym zakresie, nie są dołączane do ParryHotter.

## Runy nie działają?

1. Testuj na **nowej grze** (zapis ze starej gry może mieć postać bez szóstego kręgu magii).
2. Otwórz ekwipunek **TAB**, podświetl `Flipendo`, wybierz/użyj przedmiotu **Ctrl + strzałka w górę** (lub LPM z klawiszem do przodu, zależnie od konfiguracji). Runa powinna być wyposażona i przypisana do klawisza **4-0**.
3. Wybierz jej numer, namierz NPC i **przytrzymaj Ctrl + strzałka w górę** (standardowy schemat Gothic 1).
4. Sprawdź w statystykach, czy Parry Hotter ma **krąg magii 6** i **200 many**.
5. Jeśli próbowałeś już wersji v0.1, pobierz **najnowszy instalator**, uruchom go ponownie, w GothicStarter_mod wybierz reparse i rozpocznij nową grę. Aktualizacja dodaje postaci także `Humans_Mage.mds`, animacje rzucania czarów używane przez magów Gothic 1.

**Ważne:** ten prototyp wykorzystuje standardowe sterowanie, a nie jeszcze system jednego kliknięcia myszą. Jeśli widzisz komunikat o wymaganym kręgu albo runy się nie przypisują, zrób zrzut ekranu z ekwipunkiem i statystykami; bez niego nie da się odróżnić problemu skryptu od sterowania.

## Model z Komnaty Tajemnic (PC)

Nie został jeszcze dodany. Nasze skrypty wciąż używają `HUMANS.MDS` i podstawowego modelu postaci Gothic 1.

Do lokalnego testu potrzebny jest eksport modelu z Twojej kopii *Harry Potter i Komnata Tajemnic (PC)*, np. za pomocą [zmodyfikowanego eksportera UE Viewer](https://github.com/DramaDesu/UEViewer) obsługującego HP2 (formaty `PSK` / `PSA`). Kolejne etapy: import do Blendera, retarget szkieletu i animacji na ludzi z Gothic 1, eksport przez KrxImpExp i skonfigurowanie `MDS`/wizualizacji bohatera. Nie wystarczy skopiować pliku modelu HP2 do Gothica.

Plików modeli, animacji czy tekstur z gry Harry Potter nie publikujemy w publicznym repozytorium bez odpowiedniego zezwolenia. Można też przygotować autorski model postaci Parry'ego. 
