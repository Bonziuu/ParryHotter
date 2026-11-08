# Automatyczna aktualizacja DLL Parry Hotter

Raz rozpakuj `ParryHotter-aktualizator.zip` do folderu projektu:
`C:\Users\Bonzo\Downloads\ParryHotter-DEV013`.
Plik `AKTUALIZUJ_PARRY_HOTTER.bat` powinien byc obok `ZBUDUJ_MOD_PO_REPARSE.bat`,
a skrypty aktualizatora w podfolderze `tools`. Paczka zawiera wylacznie narzedzia;
nie zawiera modelu Harry'ego ani zasobow Gothica.

Od tej pory uruchamiaj `AKTUALIZUJ_PARRY_HOTTER.bat` przy zamknietym Gothicu.
Mozesz utworzyc do niego skrot na pulpicie. Aktualizator:

1. Pobierze DLL z wydania `dev013-latest`, sprawdzi jej SHA-256 z metadanych
   GitHuba i pliku `.sha256` oraz zgodnosc PE32/i386 i identyfikatora moda.
2. Zachowa kopie dotychczasowego `ParryHotter.mod` i lokalnej DLL przed zmiana.
   Kopie maja unikalne nazwy `*.parryhotter-backup-*`; stare kopie zostaja.
3. Podmieni tylko DLL wewnatrz istniejacego `ParryHotter.mod` i w
   `.parryhotter\build\ParryHotterHUD-G1.dll`. Sprawdzi ponownie zawartosc moda.
   Blad zapisu przywraca poprzednie pliki; kopie zapasowe pozostaja.
4. Otworzy GothicStarter_mod. Wybierz **Parry Hotter (PLAY)**.

Nie trzeba recznie pobierac/podmieniac DLL, budowac jej w Visual Studio ani
wykonywac Reparse Scripts. Aktualizator zachowuje wszystkie pozostale pliki
wewnatrz lokalnego moda, w tym skompilowane skrypty, prywatny model i tekstury.
Jesli DLL jest juz aktualna, sprawdzenie nie tworzy kolejnych kopii ani nie
przepisuje moda. Przy bledzie pobrania lub sumy kontrolnej gra nie jest uruchamiana.

Wymagania: Python 3.10 lub nowszy dostepny jako `py -3` oraz istniejacy,
wczesniej zbudowany `Gothic\Data\ModVDF\ParryHotter.mod` z DEV 0.13.
Folder gry domyslnie: `C:\Programy\Steam\steamapps\common\Gothic`.
Inna lokalizacje mozna podac tak:

```bat
AKTUALIZUJ_PARRY_HOTTER.bat --game "D:\Steam\steamapps\common\Gothic"
```

Aktualizacja dotyczy **samej DLL**. Zmiany skryptow/animacji oglaszane osobno
nadal wymagaja ich instalacji, Reparse i pelnego budowania moda.
