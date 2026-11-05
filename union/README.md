# Parry Hotter / Gothic I Union — HUD i czarowanie

Status: eksperymentalna wtyczka Union. Skompilowanie C++ to nie to samo, co sprawdzenie działania w prawdziwej grze.

## Interfejs inspirowany grami Harry Potter PC

Na przesłanym screenie Harry trzyma różdżkę, a przy celu świeci symbol zaklęcia. Parry Hotter ma własne, oryginalne ikony i stale widoczne sześć kwadratowych slotów.

| Klawisz | Zaklęcie | Symbol |
| --- | --- | --- |
| 1 | Flipendo | błękitny wir |
| 2 | Levicorpus | fioletowy hak |
| 3 | Rictusempra | złota fala |
| 4 | Expelliarmus | krzyżujące się różdżki |
| 5 | Bumbardo Maximus | pomarańczowy wybuch |
| 6 | Avada Kedavra | zielony piorun |

Celownik to dyskretna kropka w centrum. Nad NPC wskazanym natywnym systemem skupienia Gothica pojawia się symbol wybranego zaklęcia. Gdy cel znika, symbol znika.

1–6 zmieniają wybrany czar. Przytrzymany PPM ma przygotowywać różdżkę, a świeże kliknięcie LPM podczas trzymania PPM rzucić czar, jeśli trafiony został żywy NPC. Zwolnienie PPM kończy celowanie. Kamera porusza się przez system Gothic/SystemPack.

Oryginalne Harry Potter 1/2 PC mają inne sterowanie (LPM do ładowania i zwolnienie do rzucania). Tutaj celowo używamy wersji wybranej przez użytkownika.

## Pliki

- union/hud/SpellHudState.hpp — logika sześciu czarów, state machine gestów myszy i kontroli celu.
- union/Plugin/plugin.cpp — G1 zCView, native ReadySpell, Spell_Cast, fokus NPC, próba wyeliminowania konfliktu skoku na PPM.
- union/userapi/zCInput_Win32.inl — deklaracja rozszerzenia API wejścia Union.
- union/tests/test_hud.cpp — automatyczne testy stanu i sterowania.
- tools/generate_hud_textures.py — 14 autorskich tekstur 64x64 TGA (6 slotów, 6 wskazań celu, ramka, centralna kropka).
- .github/workflows/union-hud.yml — kompilacja natywnej DLL Windows/G1 oraz testy ikon i gestów.

## Eksperymentalne uruchomienie (tylko kopia gry i nieistotne zapisy)

1. Pobierz repozytorium i uruchom INSTALUJ_PARRY_HOTTER.bat (polskie źródła, zaklęcia, tekstury i backup).
2. W GothicStarter_mod wybierz Parry Hotter DEV i wykonaj Reparse Scripts. Rozpocznij nową grę, potem wyjdź.
3. Uruchom ZBUDUJ_MOD_PO_REPARSE.bat. Próbuje zbudować ParryHotter.mod z lokalnych skryptów, napisów, ikon oraz DLL pobraną z własnego wydania GitHub, sprawdzoną SHA256.
4. W GothicStarter wybierz Parry Hotter (PLAY). DLL jest umieszczona jedynie w wirtualnym System/Autorun tego moda; NIE kopiuj jej ręcznie do Gothic/System/Autorun.

Pre-release testowej wtyczki: https://github.com/Bonziuu/ParryHotter/releases/tag/hud-dev (tylko jeśli build zakończył się sukcesem).
Szablon SDK: https://github.com/Gratt-5r2/UnionProject

## Ograniczenia

- HUD i input nie są jeszcze przetestowane w uruchomionym Gothic I.
- Ruch kamerą korzysta z istniejącego sterowania SystemPack; nie został przeniesiony oryginalny kod kamery Harry Potter 2.
- Celowanie korzysta z focus NPC Gothic, nie precyzyjnego raycastu w pojedynczy piksel.
- Przygotowanie zaklęcia używa natywnych animacji magii Gothic, nie własnej animacji Harry'ego z różdżką.
- Brak konwersji modelu Harry z peleryną do działającego ASC, brak ragdolla i docelowej fizyki wyrzutu NPC.
- Kompilator GothicVDFS i ładowanie DLL w grze wymagają rzeczywistego testu lokalnego.

Jeżeli gra się zawiesza, nie używaj testowego profilu PLAY na ważnych zapisach. Uruchom zwykłego Gothica i wyślij błąd.