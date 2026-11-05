# Parry Hotter — Gothic 1 (oryginalny ZenGin)

Eksperymentalny, **nieukończony** mod do oryginalnego Gothic 1. Grasz jako **Parry Hotter** i używasz sześciu zaklęć.

## Co zawiera prototyp v0.1

| Runa | Mechanika bazowa | Efekt w v0.1 |
| --- | --- | --- |
| Flipendo | Fist of Wind | Odrzucenie i obrażenia. |
| Levicorpus | Ice Block | Zamrożenie celu (unoszenie jeszcze nie działa). |
| Rictusempra | Berserk | Oszalała agresja NPC. |
| Expelliarmus | Ball Lightning | Ogłuszenie i próba upuszczenia trzymanej broni. |
| Bumbardo Maximus | Storm Fist | Mocny odrzut bez ragdolla. |
| Avada Kedavra | Breath of Death | Animacja porażenia u ludzi, następnie zabicie; natychmiastowe zabicie potworów. |

To prototyp skryptów, **bez pliku wynikowego .mod**. Efekty opierają się na Gothic 1, nie na zasobach Harry Potter.

## Uruchomienie testowe na Windows

Wymagania: legalna instalacja oryginalnego Gothic 1, Gothic 1 Mod Development Kit z rozpakowanymi skryptami Daedalus, Player Kit z GothicStarter_mod oraz Python 3.10+.

1. Zrób kopię instalacji Gothica do eksperymentów.
2. Sklonuj lub pobierz to repozytorium.
3. Wpisz w terminalu (podmień ścieżkę na własną):

    py tools/install.py --content "C:\Gothic\_work\Data\Scripts\Content"

4. Skopiuj plik System/ParryHotter.ini z repozytorium do Gothic/System/ParryHotter.ini.
5. Uruchom GothicStarter_mod.exe, wybierz Parry Hotter (DEV), włącz ponowne parsowanie skryptów (reparse scripts), rozpocznij **nową grę** i załóż runy.
6. Jeśli coś pójdzie nie tak, wróć do kopii gry. Instalator zapisuje kopie zmodyfikowanych plików z rozszerzeniem .parryhotter.bak, ale to nie zastępuje backupu całej instalacji.

**Nie zweryfikowano jeszcze kompilacji Daedalusa ani działania w silniku.** Warianty ModKitu mogą się różnić.

## Co jeszcze nie działa

Na razie czary rzuca się standardowym sterowaniem Gothic 1, a model postaci to model zastępczy z Gothica. Osobny system myszy, modele i animacje Harry'ego oraz realistyczne wyrzucanie i obracanie NPC wymagają osobnego kodu Union (C++) i assetów.

Model Harry'ego z Komnaty Tajemnic **nie jest dołączony**. Jego pozyskanie do eksperymentów wymaga własnej kopii gry; rozpowszechnianie oryginalnego modelu i tekstur wymaga odpowiednich praw.

## Pliki

- mod/Magic/ParryHotter.d — sześć run i specjalne reakcje NPC.
- tools/install.py — instalator modyfikujący lokalne skrypty źródłowe.
- System/ParryHotter.ini — profil deweloperski.
- tests/test_install.py — automatyczne testy instalatora.
- ROADMAP.md — plan kolejnych etapów.

Oryginalna gra, ModKit i zasoby innych gier nie są tu dystrybuowane.
