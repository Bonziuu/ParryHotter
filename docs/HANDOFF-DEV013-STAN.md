# Parry Hotter DEV 0.13: stan prac i przekazanie (8.11.2026)

Gałąź robocza: `codex/dev013-handoff` (repozytorium `Bonziuu/ParryHotter`).
CI (`.github/workflows/union-hud.yml`) buduje `ParryHotterHUD-G1.dll` przy każdym
pushu i publikuje ją jako prerelease `dev013-latest` (DLL + `.sha256`).

## Środowisko u użytkownika

- Gothic I Classic (Steam): `C:\Programy\Steam\steamapps\common\Gothic`. Union 1.0m.
  Gra uruchamia się przez `System\GothicStarter_mod.exe`, a właściwy proces to
  `System\GothicMod.exe` (adresy funkcji silnika = Gothic 1.08k, sprawdzone
  disasemblacją).
- Profile: `System\ParryHotter.ini` (DEV, `-zreparse -zwindow`) i
  `System\ParryHotter-PLAY.ini` (gra, `-zwindow`).
- Projekt: `C:\Users\Bonzo\Downloads\ParryHotter-DEV013\`. DLL trafia do
  `.parryhotter\build\ParryHotterHUD-G1.dll`, a `ZBUDUJ_MOD_PO_REPARSE.bat`
  pakuje mod.
- Dziennik diagnostyczny: `Gothic\ParryHotter_cast.log` (nadpisywany przy
  każdym starcie). Od ostatniego commita jest otwierany przez `_fsopen` z
  `_SH_DENYWR`, więc można go czytać w trakcie gry.

## Co działa (potwierdzone na filmach i w dzienniku)

1. **Model Harry'ego (HP2)** jest tylko na bohaterze, a menu jest po polsku.
   Ciemne spodnie pod szatą: `tools/harry_trousers.py` przemalowuje teksele
   nóg w `PH_HARRY_TEX0.TGA` (oryginał zapisuje jako `.parryhotter-original-legs`).
2. **HUD**: 10 slotów, sloty 1–6 to zaklęcia, a 7–0 są puste (decyzja użytkownika).
3. **Hooki**: metody wirtualne muszą być hookowane po ADRESIE (makro `PH_VIRTUAL`
   w `union/Plugin/plugin.cpp`). Wskaźnik do metody wirtualnej to w MSVC thunk
   vcall i hook nigdy się nie wywoływał.
4. **Wejście**: Gothic wywołuje `ProcessInputEvents` około 16 tys. razy na
   sekundę i każde wywołanie zeruje delty myszy oraz tablicę KeyToggled.
   Dlatego mysz jest sumowana (`pendingMouseX/Y`), a klawisze 1–0 są
   zatrzaskiwane (`ConsumeSlotPressed`).
5. **Kamera HP2**: `PlaceHp2Camera` (plugin.cpp) ustawia kamerę 300 cm za
   pivotem 75 cm nad głową Harry'ego, w kierunku yaw/pitch z myszy. Kolizja
   przysuwa kamerę, a swobodne rozglądanie działa także w biegu.
6. **Ruch**: 540 cm/s, kierunek względem kamery. W bok i do tyłu grają
   animacje strafe/back. Zabezpieczenie przed wpadnięciem pod teren:
   `lastSafePosition`.
7. **Namierzanie**: kropka, aim assist, ikonka (glyph) nad NPC, dźwięk
   `MFX_LIGHT_CAST` i fokus z imieniem działają.
8. **Animacje rzucania Harry'ego**: overlay `PH_HARRY_CAST.MDS` musi używać
   ISTNIEJĄCYCH nazw z `HUMANS.MDS` (`T_MAGRUN_2_WNDCAST`, `S_WNDCAST`,
   `T_WNDCAST_2_WNDSHOOT`, `S_WNDSHOOT`, `T_WNDSHOOT_2_STAND` oraz to samo dla
   FIB, a dla FRZ i SLE wersje *SHOOT). Wersja z nowymi nazwami była przez
   silnik odrzucana w całości („ani in overlay not present in base”). Dziennik
   potwierdza `overlay=1`.
9. **Litery animacji czarów** (`spellFXAniLetters`): Windfist i Stormfist to
   WND; Thunderball i BreathOfDeath to FIB; IceCube to FRZ; Berzerk to SLE.

## Rzucanie zaklęć: stan i wiedza z disasemblacji

Natywny Gothic I (`oCAIHuman::MagicMode` @ `0x0046D260`):

- Ładowanie (`MagicInvestSpell` @ `0x0046C330`) trwa, gdy logiczne klawisze
  `GAME_ACTION` (5) i `GAME_UP` (3) są jednocześnie wciśnięte, co sprawdza
  `oCAIHuman::Pressed`, czyli `zinput->GetState`.
- Wystrzał następuje po PUSZCZENIU klawiszy, jeśli `spell->manaInvested > 0`
  (offset 0x48). Wtedy idzie wiadomość do EventManagera, a potem
  `MagicCheckSpellStates` (@ `0x0046C940`) przy aktywnej animacji shoot
  wywołuje `oCSpell::Cast`.
- `MagicInvestSpell` NIC nie robi, gdy postać gra animację chodu (`IsWalking`).

Implementacja w pluginie (`DriveNativeMagic` w `plugin.cpp`, wywoływana w
`oCAIHuman::PH_DoAI` tuż przed natywnym AI gracza):

- Przy zaklęciach ładowanych (1, 4, 5, 6) klawisze ACTION+UP są trzymane, gdy
  LPM jest wciśnięty i kropka jest na celu (`gChargeKeys`). Puszczenie na celu
  wywołuje `CastAtNpc`, które ustawia tylko żądanie (`gRequestedSpell`, cel,
  fokus) i zwalnia klawisze, gdy `manaInvested >= 1`. Puszczenie obok celu
  wywołuje `LowerWand`, czyli anulowanie.
- Przy zaklęciach natychmiastowych (IceCube, Berzerk) klawisze są wciskane po
  puszczeniu LPM i zwalniane, gdy status osiągnie CAST (2).
- Podczas rzucania `ParryControls::castPose` wymusza `_Stand()`, bo Gothic
  nie ładuje czaru w trakcie chodu.

Ostatni test (przed commitem `fca61b4`): IceCube doszedł do statusu 2 i zużył
4 many, ale nie wystrzelił, bo klawisze nie zostały zwolnione. Thunderball nie
ładował się, bo Harry szedł. Oba problemy są poprawione w `fca61b4`, ale tej
wersji NIE przetestowano w grze. Następny krok to test w grze i odczyt
dziennika (szukaj linii `charge`, `cast ... REQUEST`, `native status=`,
`engine Cast`).

## Znane problemy i TODO

1. **Potwierdzić w grze poprawkę po logu `b25528b`** opisaną niżej. Testy
   przenośne i build DLL nie potwierdzają jeszcze wystrzału w ZenGin.
2. **Berzerk**: `IsValidTarget` zwraca 0 dla strażnika. To może być reguła
   czaru, do sprawdzenia.
3. **Konsola Gothica (F2, Marvin)**: przy otwartej konsoli plugin dalej czyta
   WASD i mysz. Do `ParryControls::ContextAvailable` trzeba dodać
   `zCConsole::cur_console && zCConsole::cur_console->IsVisible()`
   (`0x006DA710`). Mock w `union/tests/test_player_controls.cpp` musi wtedy
   dostać `zCConsole`.
4. **Kamera pod dachem**: przy przysunięciu (min. 40 cm) pivot 75 cm nad głową
   wypycha Harry'ego z kadru. Przy małym dystansie pivot trzeba obniżać
   proporcjonalnie (np. `kAboveHead * dist/300`).
5. **Skok**: to wciąż długi skok Gothica. Docelowo niski skok jak w HP2.
6. **Animacja chodu z czarem**: dziś Harry „ślizga się” w pozie stojącej.
   Potrzebna animacja nakładkowa dolnej części ciała albo tylko górna warstwa.
7. **Efekty zaklęć HP2**: Levicorpus (lewitacja do góry nogami), Expelliarmus
   (rozbrojenie), Bumbardo, Avada, a także ragdoll i fizyka.
8. **Dokumentacja i finalna paczka** z instrukcją po polsku. Decyzja o merge
   do `main` należy do użytkownika.

## Analiza przesłanego logu `b25528b` i poprawka wejścia

Log zawiera siedem `REQUEST`: slot 0 dwa razy, slot 1 dwa razy, slot 2 raz
i slot 3 dwa razy (numeracja logu jest od zera). Nie ma ani jednego `engine Cast`
ani `native CAST blocked`. Slot 0 przy 22.674 s miał `status=1 invested=1`,
a przy 22.676 s `status=2 keys=0`, lecz `castAni=0`; żądanie wygasło po 4 s.
Slot 2 przy 96.485 s miał `invested=-1`, co oznacza anulowanie przez natywne
`oCSpell::Stop`, nie stan gotowości czaru natychmiastowego. Dla slotów 4 i 5
wszystkie próby miały `target=0`, więc log nie weryfikuje rzutu tych czarów na NPC.

Disasemblacja GothicMod.exe 1.08k potwierdziła:

- `zCInput_Win32::SetState` @ `0x004C83E0` ustawia `zSKeyMapping::set`.
  Wartość zero usuwa wymuszenie, ale `GetState` @ `0x004C8310` nadal czyta
  fizyczne klawisze. Zatem czyszczenie `GAME_UP` nie blokowało trzymanego W.
- `MagicCheckSpellStates` wymaga aktywnej animacji `s_shoot`, aby wywołać
  `Spell_Cast`. Natywny bieg i `_Stand` mogły zastępować tę animację.
- `IsValidTarget` @ `0x0047DD80` sprawdza kąty względem bohatera, własny zasięg
  czaru i LOS. Kropka kamery może więc wskazywać NPC poza natywnym stożkiem.

Poprawka hookuje wirtualne `GetState` po adresie przez `PH_VIRTUAL`. Tylko
w obrębie natywnego AI gracza maskuje akcje ruchowe adaptera, a tylko w obrębie
jego `MagicMode` podaje syntetyczne ACTION+UP. Poza tymi zakresami oraz dla NPC
wejście pozostaje natywne. W trybie F11 natywny ruch jest wstrzymywany na czas
ładowania/żądania. Aktywne animacje przygotowania i rzutu są chronione przed
`_Stand`; flaga ładowania jest zerowana przy zmianie czaru. Sam wystrzał zawsze
wymaga zgodnego żądania puszczenia LPM, również wewnątrz własnego wywołania AI.
Przy pełnym naładowaniu getter statusu pokazuje natywnemu `MagicMode` stan
`CANINVEST` do czasu żądania rzutu; rzeczywisty status `CAST`, mana i efekt nie
są zmieniane. Kolejne inwestowanie jest wtedy wstrzymane. Zapobiega to
automatycznemu przejściu do shoot i skonsumowaniu czaru podczas trzymania LPM.

`SpellTargeting.hpp` na czas sprawdzania/obsługi własnego czaru ustawia zasięg
1900 cm i pełne kąty. Zachowuje natywne filtry typu celu oraz LOS. Parametry są
przywracane przed zwolnieniem referencji; nie zmienia czarów NPC ani skryptów gry.

Nowa DLL zapisuje `build PH_CAST_INPUT_SCOPED_GETSTATE_V1`, zmiany `magic before`
i `magic after` z rzeczywistymi odczytami ACTION/UP, stanem animacji i broni.
Anulowanie przez silnik ma osobny wpis `CANCELLED by engine`, a przekroczenie
czasu `TIMEOUT`. Każde nowe żądanie zapisuje początkowe `native status`.

Po podmianie DLL przetestować wszystkie sześć czarów: stojąc, przy trzymanym W,
po zmianie czaru podczas LPM i po obrocie kamery; osobno puścić LPM bez celu.
Oczekiwane potwierdzenie to `engine Cast ... result!=0 sameRequest=1` oraz
rzeczywisty efekt na namierzonym NPC. Nadal potrzebny jest nowy log z gry.

Binarka do analizy pozostała poza repozytorium. Źródło: PlayerKit w
`PhoenixTales/gothic-devkit`, commit `0b81c1932d11fd212c877cb8fd4e4a85ab2584de`,
obiekt Git `a22a2f21877f8f1ecf11f9878f21a7c142b73566`; SHA-256:
`af5859155fcde73a2923f3db4ce608f77f5e998413a305d462c44e80c76de1e0`.

### Kolejna próba: log z 9 listopada, DLL V1

Nowy log zawiera znacznik `PH_CAST_INPUT_SCOPED_GETSTATE_V1`. Są **6 REQUEST**:
Flipendo (slot 0), Levicorpus (1), Rictusempra (2), Bumbardo (4, dwa razy)
i Avada (5). Expelliarmus (3) ma tylko rozpoczęcie ładowania. Cztery żądania
kończą się `TIMEOUT`, dwa `CANCELLED by engine`; nie ma żadnego `engine Cast`
ani nawet `magic before/after`. Ochrona animacji nie wystarczyła do naprawy.
Film nie został przeanalizowany: przesłany plik przekracza limit transferu
32 MiB narzędzia pobierania.

Disasemblacja potwierdza, że `ReadySpell` nie ustawia trybu broni od razu:
kolejkuje `oCMsgWeapon(0, 7, 0)` (adresy `0x0074B049–0x0074B073`).
`PC_ActionMove` wywołuje `MagicMode` dopiero przy `GetWeaponMode() > 6`
(`0x00613F56–0x00613F76`). Sam `TransitionToInvest` uruchamia animacje,
ale nie ustawia tego trybu. Nie potwierdzono, która bramka natywnego AI
zatrzymuje tę konkretną sesję; wcześniejszy log nie zawierał trybu broni.

DLL V2 usuwa przedwczesny `TransitionToInvest` z `RaiseWand`: poprawne przejście
uruchomi natywny `MagicInvestSpell` po przygotowaniu broni. Nie wymusza
`SetWeaponMode` i nie usuwa wiadomości silnika. Cztery hooki magii mają jawne,
zweryfikowane adresy G1 1.08k, aby usunąć niejednoznaczność wskaźników SDK.
Nie stwierdzono, że sam wskaźnik SDK był jedyną przyczyną.

Identyfikator nowej DLL: `PH_CAST_INPUT_NATIVE_READY_V2`. Nowe wpisy
`player AI before/after` powstają poza `MagicMode`: pokazują `npcMode`,
`aiMode`, `actionMode`, `changeWeapon`, `scriptStateValid`, `scriptState`, `castAni`, `magicCalls`
i wskaźnik `caster`. Początek logu pokazuje także adresy hooków. Dzięki temu
nawet brak wejścia do `MagicMode` zostawi informacje o miejscu zatrzymania.
Konieczny jest test w grze po automatycznej aktualizacji DLL; build i testy
przenośne nie dowodzą jeszcze wystrzału.

## Jak testować u użytkownika

Automatyczna aktualizacja samej DLL: rozpakować z `dev013-latest` paczkę
`ParryHotter-aktualizator.zip` do folderu projektu w Pobranych i uruchamiać
`AKTUALIZUJ_PARRY_HOTTER.bat`. Pobiera i weryfikuje DLL, robi trwałe kopie,
podmienia wyłącznie jej wpis wewnątrz istniejącego `.mod` oraz lokalny cache DLL,
a następnie otwiera GothicStarter. Nie wymaga Git, Visual Studio ani Reparse;
wymaga zamkniętej gry i Pythona. Pełna instrukcja:
[AKTUALIZACJA-DLL.md](AKTUALIZACJA-DLL.md).

Ręczna ścieżka, przydatna również po zmianach skryptów lub zasobów:

1. Pobrać DLL z prerelease `dev013-latest` i sprawdzić `.sha256` oraz
   `tools/verify_dll.py`.
2. Podmienić `.parryhotter\build\ParryHotterHUD-G1.dll`.
3. Uruchomić `ZBUDUJ_MOD_PO_REPARSE.bat`, aby lokalna DLL trafiła do `.mod`.
   Gdy zmieniły się skrypty, najpierw wykonać Reparse. Sama podmiana DLL nie
   wymaga Reparse; nowy aktualizator podmienia ją bez pełnego budowania moda.
4. Uruchomić GothicStarter_mod, wybrać profil **Parry Hotter (PLAY)** i
   rozpocząć nową grę.
5. Do testów można użyć konsoli: B, wpisać `marvin`, B, potem F2 i
   `insert molerat` (konsola sama uzupełnia `insert` po literze „i”).
6. Odczytać `Gothic\ParryHotter_cast.log`.

## Zasady od użytkownika

- Pisać zawsze po polsku.
- Sterowanie jak w HP2 PC:
  - LPM: przytrzymanie celuje, puszczenie rzuca;
  - PPM: skok, bez automatycznego skoku;
  - kropka na środku ekranu i swobodna kamera;
  - WASD bez obracania postaci.
- Bez namierzonego NPC zaklęcie się nie wystrzeliwuje. Zasięg 19 m zostaje.
- Model i oryginalne zasoby trzymać poza publicznym repozytorium (tylko
  lokalnie).
- Nie wyłączać weryfikacji TLS ani sum kontrolnych. Zachowywać kopie
  zapasowe plików gry.

## Sterowanie i diagnostyka V3 (13.11.2026)

Zintegrowano kolejkę zdarzeń myszy/cyfr, migawkę celu z puszczenia LPM, osobny
slot rzutu oraz oczekiwanie na gotowość natywnej magii po krótkim kliknięciu.
Zachowano zasięg 1900 cm i odmowę bez żywego, widocznego NPC. Rzut sprawdza
ponownie cel i kontekst tuż przed oryginalnym Cast. Konsola przekazuje wejście
Gothicowi i blokuje skróty moda; po przerwaniu trzeba puścić przyciski.

Nowa kamera obniża pivot przy przeszkodzie i pod dachem oraz śledzi przekrój
near clip. Skok ma jednorazowy impuls na natywnej fizyce, docelowo 70 cm, bez
ponawiania z przytrzymania. Ruch korzysta z rzeczywistych wyników kolizji;
opadanie nie wykonuje już ręcznych przesunięć w dół. Proceduralna korekta
kości nóg umożliwia ruch w pozie czaru bez uruchamiania IsWalking. Nowy raw hook:
`zCModel::AdvanceAnis @ 0x00562CD0` (główna pętla blendowania jest tam inline).
Skok i cykl nóg są autorskimi przybliżeniami wymagającymi próby w grze.

Znacznik: `PH_CONTROLS_V3_EVENTS_READY`. Log zawiera sekwencje wejścia, ID żądań,
wynik Cast i opóźnienie, koszty/inwestowanie many, przyczyny anulowań, ID/fazy
skoków z wysokością/czasem oraz próbki ruchu i kamery 2 Hz. Bufor linii 1024 B,
limit sesji 4 MiB. Narzędzie `python -m tools.analyze_controls_log PLIK --json`
podsumowuje obserwacje; brak logu sukcesu nie jest traktowany jako sukces.
Scenariusze: [TESTY-STEROWANIA-HP2.md](TESTY-STEROWANIA-HP2.md).

W dotychczasowym drzewie repozytorium znaleziono także archiwum modelu Harry'ego.
Usunięto je z bieżącej gałęzi i wykluczono przez .gitignore; zachowana lokalna
identyczna kopia w środowisku roboczym: `/workspace/ParryHotter-local-assets/`.
Nie zmieniano zasobów ani kopii bezpieczeństwa w instalacji Windows użytkownika.

Pełne efekty HP2 i ragdoll nadal pozostają osobnym zadaniem. Ten zestaw zmian
nie potwierdza ich ukończenia ani zgodności sterowania 1:1 bez testu w Gothic.

## Log V3 i poprawka V4: ponawianie czarów oraz różdżka

Nowy log użytkownika zawiera 9 REQUEST, 1 potwierdzony Cast (Flipendo),
4 timeouty i 4 anulowania. Jedyny Cast wystąpił przy 28,695 s, 339,6 ms po
puszczeniu LPM. Wszystkie `native Invest` są przed tym Cast; później silnik
wchodzi do MagicMode z ACTION=1/UP=1, lecz nie inwestuje ani razu.

Przyczyna potwierdzona w GothicMod.exe 1.08k:

- `MagicCheckSpellStates @ 0x0046CBDD` ustawia bit 8 `spellCastedLastFrame`
  w polu AI +0x1204 po rzucie.
- `MagicMode @ 0x0046D363` omija kolejne MagicInvestSpell, gdy bit jest ustawiony.
- Wyzerowanie bitu jest na ścieżce ACTION=1, UP=0
  (`0x0046D3A1`, AND 0xFFFFFEBF). Zwolnienie obu przycisków nie dochodzi do niej.

V4 rozdziela oba klawisze. Po puszczeniu LPM ACTION pozostaje aktywne w zakresie
MagicMode, a UP jest zwolnione. Jeśli następne przygotowanie zastanie bit 8,
pierwsze wywołanie MagicMode dostaje natywną sekwencję ponownego uzbrojenia.
Intencja ładowania pozostaje zachowana na następne wywołanie. Nie modyfikujemy
bitu pamięci silnika ręcznie. Log zawiera `lastCast` i `rearm`.

Druga przyczyna widocznego opóźnienia: ReadySpell zawsze kolejkuje DRAWWEAPON,
a UnreadySpell REMOVEWEAPON. `NativeWand.hpp` wybiera/zarejestrowuje rzeczywistą
runę i używa `SetWeaponMode2 @ 0x00695A60` do natychmiastowego trybu magii,
bez wiadomości wyciągania/chowania broni. Ta metoda sama aktualizuje natywne
overlays, animacje walki i chodu. `Spell_Setup @ 0x0046F800`, opakowane w Scope
celowania, ustawia caster/cel/efekt przed pierwszym AI. Zakończenie używa
StopSelectedSpell i natychmiastowego przywrócenia trybu zamiast UnreadySpell.

Istotne szczegóły SDK: `GetSpell(int)` przyjmuje indeks, nie ID; iteracja
kończy się przed GetNoOfSpells. StopSelectedSpell wymienia natywny obiekt czaru
w tym samym indeksie, więc wszystkie wskaźniki muszą być pobrane ponownie.
Wmode AI (1/2/3) opisuje wybór animacji broni; nie jest odpowiednikiem fmode=7.

Znacznik nowej diagnostyki: `PH_CAST_V4_INSTANT_WAND_REARM`. Kolejna próba musi
obejmować przynajmniej trzy kolejne rzuty tym samym czarem i powtórzenie dla
wszystkich sześciu slotów. Kompilacja i regresje nie dowodzą jeszcze działania
V4 w grze użytkownika. Paczka aktualizatora zawiera teraz również analizator
logów i instrukcję prób.
