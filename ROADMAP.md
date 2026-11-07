# Parry Hotter — DEV 0.13

## Zaimplementowane w kodzie

- 10 stałych kwadratowych pól, etykiety 1–9 i 0; 6 istniejących czarów, 4 puste pola bez rzutu.
- PPM przygotowuje czar, nowy LPM rzuca; znak widoczny podczas celowania, animowany przy rzutowanej pozycji NPC.
- Ruch względem kamery, szybkie osiem kierunków, sterowanie w powietrzu i bufor skoku; natywne kolizje i grawitacja.
- Swobodny obrót myszą przez parametry kamery silnika; F11 przywraca standardowy ruch/kamerę.
- Potwierdzanie rzutu przez natywny oCSpell::Cast; odnowienie dopiero po sukcesie, obsługa opóźnienia animacji.
- Rejestr wielu pocisków według rzeczywistej tożsamości efektu i ofiary; ograniczony czas życia i pojedynczy impuls na rzut.
- Eksport ASC przez zainstalowany KrxImpExp, normalizacja wag, dopasowanie głowy/peleryny, walidacja geometrii i tekstur.
- Usunięcie oryginalnej głowy PC_Hero podczas instalacji; kod zachowania wyglądu po zmianie pancerza.
- Własne klipy przygotowania/rzutu różdżką, 25 kości, 30 FPS; opcjonalna nakładka PC_Hero i sześć natywnych uchwytów z przywracaniem.
- Pakowanie odrzuca starą DLL i niepełny DAT; przypięty SDK i lokalny skrypt kompilacji Windows.

## Zweryfikowane poza grą

- Testy Python i przenośne testy C++17: python -m tools.check.
- Kompilacja kontrolna prawdziwych Unified-PL + ParryHotter przez przypięty GDC na Linux; wariant stroju również.
- Syntetyczny FBX → wagi szkieletu → rzeczywisty eksport Kerrax ASC → staging i walidacja w Blenderze 4.3.2.
- Symulacja adaptera ruchu sprawdza kolejność wywołań i zarządzanie wejściem; nie potwierdza ABI silnika.
- Rzeczywiste klipy różdżki: eksport i ponowny import Kerrax, zamknięcie pętli, zgodność szkieletu, brak ruchu korzenia; podgląd na prawdziwym modelu.

## Wymagane przed grywalnym wydaniem

- [x] Rzeczywisty Harry HP2 PC przekonwertowany na 25 kości prawdziwego szkieletu Gothica; ASC i cztery TGA zwalidowane. Próby głowy/ręki/kolana oraz instalacji źródeł przeszły. Referencja ModKitu jest pobierana z przypiętego źródła i weryfikowana SHA-256.
- Sprawdzenie deformacji twarzy, włosów, szaty, peleryny i dłoni na tym modelu.
- Kompilacja aktualnej DLL przez MSVC/Win32; skonfigurowane CI nie jest dowodem jej wykonania.
- Parser ZenGin, kompilacja modeli/tekstur, GothicVDFS i uruchomienie końcowego .mod.
- Próby kamery, kolizji, skoków/sterowania w powietrzu, rozmów i przejść między światami.
- Próby rzeczywistych efektów każdego czaru, many, trafień kilku pocisków i zmiany celu.
- Próba własnych animacji różdżki w Gothicu, pełny zestaw ruchów HP, odwrócone zawieszenie Levicorpus i pełny ragdoll/obroty Bumbardo.
- Dalsze strojenie szybkości, czułości, wysokości skoku i efektów na podstawie gry.

Lista prób i polecenia: [docs/DEV-013.md](docs/DEV-013.md).
