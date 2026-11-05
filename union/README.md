# Parry Hotter / Gothic I Union — HUD i czarowanie

Status: eksperymentalna wtyczka Union. Skompilowanie C++ to nie to samo, co sprawdzenie działania w prawdziwej grze.

## Interfejs inspirowany grami Harry Potter PC

Na przesłanym screenie Harry trzyma różdżkę, a przy celu świeci symbol zaklęcia. Parry Hotter ma własne, oryginalne ikony i stale widoczne sześć kwadratowych slotów.

| Klawisz | Zaklęcie | Symbol |
| --- | --- | --- |
| 1 | Flipendo | błękitna fala odrzutu |
| 2 | Levicorpus | fioletowy hak |
| 3 | Rictusempra | ognista spirala jak na screenie z HP2 |
| 4 | Expelliarmus | krzyżujące się różdżki |
| 5 | Bumbardo Maximus | pomarańczowy wybuch |
| 6 | Avada Kedavra | zielony piorun |

Celownik to dyskretna kropka w centrum. Nad NPC wskazanym natywnym systemem skupienia Gothica pojawia się symbol wybranego zaklęcia. Gdy cel znika, symbol znika.

1–6 zmieniają wybrany czar. Przytrzymany PPM ma przygotowywać różdżkę, a świeże kliknięcie LPM podczas trzymania PPM rzucić czar, jeśli trafiony został żywy NPC. Zwolnienie PPM kończy celowanie. Kamera porusza się przez system Gothic/SystemPack.

Oryginalne Harry Potter 1/2 PC mają inne sterowanie (LPM do ładowania i zwolnienie do rzucania). Tutaj celowo używamy wersji wybranej przez użytkownika.

## Etap 1 — kamera i sterowanie (v0.5 DEV)

- Przy PPM kamera zbliża się do bohatera i przesuwa nad jego ramię. Zoom i offset przywracają poprzednie wartości po puszczeniu PPM, w czasie pauzy i przed opuszczeniem świata.
- Wykrywanie celu odrzuca martwe i odległe NPC (powyżej około 19 metrów). Dodatkowo używa **raycastu ze środka kamery**, aby symbol nie pojawiał się przez ściany lub poza centrum ekranu. NPC nadal musi być wykryty przez standardowy focus Gothica — to wariant prototypowy do testów.
- W czasie ekwipunku, przejść między lokacjami i scenek przerywnikowych skrypt nie powinien przygotowywać ani rzucać czarów.
- Poprawiono instalator: `Gothic2_Control=1` nie jest już wymuszane w globalnym `SystemPack.ini`. Jeśli stara wersja Parry Hotter wcześniej zmieniła ten klucz z 0 na 1, a istnieje kopia `.parryhotter-original`, poprzednia wartość jest bezpiecznie przywracana. `enableMouse=1` pozostaje wymagane.
- Jednoczesne rozglądanie myszą i manipulacja różdżką korzystają z bazowej obsługi kamery Gothica. To nie jest jeszcze pełny swobodny obrót 1:1 z HP2. Finalna kamera powinna również obsługiwać obracanie postaci względem kamery.

**Stan:** kod natywnej DLL i testy automatyczne mogą przechodzić, ale działanie w grze musi zostać potwierdzone. W razie problemu użyj zwykłego profilu Gothic lub profilu DEV bez DLL.

## Etap 4g — kolejność przycisków i pauza (DEV 0.12)

Sterowanie wymaga teraz rzeczywistego przygotowania czaru przez PPM **przed** nowym naciśnięciem LPM. Jednoczesne PPM + LPM oraz LPM trzymany jeszcze przed PPM nie wyzwalają zaklęcia. Po zwolnieniu LPM, wciąż trzymając PPM, kolejny pojedynczy klik rzuca czar tylko na poprawny cel.

Po przerwaniu gry menu, wczytywaniem, pauzą lub zapisem, przygotowanie zaklęcia zostaje anulowane. Zanim znów zaczniesz celować, **puść PPM i naciśnij go ponownie**. Nowe testy `union/tests/test_hud.cpp` obejmują przypadkowe kombinacje przycisków oraz powrót z menu przy obu wciśniętych klawiszach.

Wcześniej zmiana slotu klawiszami 1–6 mogła próbować wywołać `ReadySpell` nawet po zablokowaniu celowania; to również poprawiono. Kontrolujemy wejście gry, ale faktyczna animacja i chwila wykonania `Spell_Cast` nadal wymagają testu na komputerze z Gothic 1.

## Etap 4f — celowanie zgodne ze środkiem ekranu (DEV 0.11)

Promień z kamery rozpoznaje teraz **pierwszego NPC pod celownikiem**, bez wcześniejszego wymagania, by mechanizm focus Gothic 1 wybrał dokładnie tego samego NPC. Aby zachować natywny system zaklęć, tuż przed `Spell_Cast()` cel zostaje tymczasowo wskazany przez `oCNpc::SetFocusVob`. Odległość i ściany nadal ograniczają rzut, a martwe NPC nie są akceptowane.

Pomocniczy panel **F10** ma nowy wiersz „Cel” z powodem odmowy rzutu (przeszkoda, martwa postać, brak kamery lub księgi magii, przekroczony zasięg). Pozwala odróżnić problem celowania od problemu z natywną fizyką.

**Test w silniku nadal konieczny:** musimy sprawdzić, jak Gothic I interpretuje `SetFocusVob` przy przygotowanych zaklęciach oraz czy przybliżenie kamery i środek promienia działają razem na prawdziwej mapie.

## Etap 4e — diagnostyka fizyki w samej grze (DEV 0.10)

Naciśnij **F10** podczas testowej rozgrywki w profilu PLAY. Pojawi się pomocniczy panel liczący:
- rozpoznane trafienia Levicorpus/Bumbardo,
- impulsy faktycznie nadane natywnej fizyce,
- wykryte lądowania,
- pominięcia z powodu wyłączonej fizyki NPC, braku `zCRigidBody` lub zablokowanej drogi.

Naciśnij **F10** ponownie, aby zamknąć panel. Domyślnie nie jest widoczny. Jest to element diagnostyczny w silniku `zCView`, bez zewnętrznych okien, bez wysyłania danych ani zapisów na dysku. Każde rozpoznane trafienie jest liczone tylko raz na rzut czaru.

Przy otwarciu ekwipunku, rozpoczęciu cutscenki czy wczytywaniu świata wtyczka przerywa niedokończoną obserwację lotu. Nie próbuje wówczas odtwarzać animacji lądowania. Zabezpiecza też przed wielokrotnym przykładaniem impulsu z obrażeń rozłożonych w czasie.

**Interpretacja:** „fizyka wyłączona” oznacza, że NPC nie miał natywnej fizyki już w momencie trafienia. Celowo jej nie aktywujemy z kodu bez sprawdzenia w Gothic I. Licznik zerowy po prawidłowym rzuceniu czaru może oznaczać brak natywnego zdarzenia `OnDamage_Hit`, inną kolejność wywołań lub brak rzeczywistego trafienia — potrzebny test w grze.

## Etap 4d — wykrywanie lądowania (DEV 0.9)

Wtyczka obserwuje NPC najwyżej przez 4,6 sekundy **po skutecznym impulsie `zCRigidBody`**. Wykrycie bezpiecznego podłoża wymaga promienia od NPC w dół, normalnej wielokąta, małej odległości i stabilnej prędkości pionowej w dwóch kolejnych klatkach (po co najmniej 450 ms od trafienia). Dopiero wtedy Bumbardo próbuje uruchomić `T_STAND_2_LIGHTNING_VICTIM` jako krótką reakcję na lądowanie.

Levicorpus nie otrzymuje drugiej animacji po lądowaniu, bo `ZS_PH_Levicorpus` ma własny limit około czterech sekund i powrót do rutyny. Jeśli NPC umrze, zmieni się świat, nastąpi zapis albo minie 4,6 sekundy, wtyczka usuwa obserwację; nigdy nie wyłącza grawitacji, nie teleportuje NPC i nie wymusza upadku.

**Zapis/zewnętrzny profil:** podczas zapisu `PH_UNION_LOADED` jest przejściowo zerowane, a po zapisie przywracane. Dzięki temu stare zapisy mogą zostać otwarte w profilu DEV bez DLL. Wymaga ponownego **Reparse Scripts** i przebudowania `.mod` po pobraniu v0.9.

**Stan:** testy C++ i kompilacja Win32 nie stanowią dowodu działania animacji w ZenGin. Dodatkowy impuls i obserwacja lądowania są nieaktywne dla NPC bez już aktywnego natywnego `zCRigidBody`; wówczas działają tylko dotychczasowe animacje Daedalusa.

## Mostek zdarzeń trafienia do Daedalusa (DEV 0.7)

HUD Union zapisuje numer ostatniego **rzucanego** zaklęcia w `PH_SELECTED_SPELL` (1–6), odczytywanym przez `B_AssessMagic.d`. Dzięki temu trafienia od Levicorpus, Rictusempra, Expelliarmus, Bumbardo i Avada mogą być rozpoznawane odrębnie od czarów bazowych, które używają tych samych identyfikatorów ZenGin. Tag wygasa po 7 sekundach i jest czyszczony przy zmianie świata; to rozwiązanie prototypowe dla pojedynczego ostatnio rzuconego zaklęcia — nie obsługuje idealnie wielu pocisków jednocześnie.

**Bumbardo Maximus** zachowuje odrzut Stormfist i dodaje krótką reakcję wstrząsu trafionego ludzkiego NPC. Brakuje dodatkowego bezpiecznego wyrzucenia/obrotu z wykrywaniem kolizji i testu prawdziwego trafienia. Słabszych NPC może zabić normalny `BreathOfDeath` przed animacją Avada; nie zmniejszamy obrażeń globalnie, bo wpływałoby to na cały świat.

Po aktualizacji skryptów **Reparse Scripts jest obowiązkowy**. Lokalny builder odmówi zapakowania `Gothic.dat` bez `PH_SELECTED_SPELL`. W wersji DEV bez wtyczki Union zostaje stary sposób identyfikacji czaru bazowego. Nie gwarantuje to odróżnienia run od innych bazowych zaklęć.

## Różdżka (nowy etap)

Podczas przytrzymania PPM wtyczka próbuje zamocować `PH_WAND.3DS` do istniejącego gniazda kości `ZS_RIGHTHAND`. Jeśli gniazdo ma już przedmiot, niczego nie nadpisuje; po zwolnieniu PPM odłącza własny model. Lokalna paczka `.mod` zawiera `_work/Data/Meshes/PH_WAND.3DS` i `_work/Data/Textures/PH_WAND.TGA`, przygotowane przez `tools/wand_asset.py`.

Instalator może zachować geometrię z oddzielnego oryginalnego [modelu różdżki HP2 PC](https://models.spriters-resource.com/pc_computer/harrypotterthechamberofsecrets/asset/291995/) **wyłącznie na komputerze użytkownika**, albo automatycznie wygenerować autorską różdżkę 3DS/TGA offline.

**Ważne:** mimo zielonej kompilacji DLL i testów plików 3DS nie zweryfikowano jeszcze położenia różdżki względem dłoni i animacji w ZenGin.

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
- Celowanie korzysta z native focus NPC oraz promienia ze środka kamery; wymagane testy, czy bryła postaci Gothic prawidłowo reaguje na trafienia.
- Przygotowanie zaklęcia używa natywnych animacji magii Gothic, nie własnej animacji Harry'ego z różdżką.
- Brak konwersji modelu Harry z peleryną do działającego ASC, brak ragdolla i docelowej fizyki wyrzutu NPC.
- Kompilator GothicVDFS i ładowanie DLL w grze wymagają rzeczywistego testu lokalnego.

Jeżeli gra się zawiesza, nie używaj testowego profilu PLAY na ważnych zapisach. Uruchom zwykłego Gothica i wyślij błąd.