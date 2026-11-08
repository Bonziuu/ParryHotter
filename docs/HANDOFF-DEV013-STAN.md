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

1. **Przetestować `fca61b4`** (rzucanie). Jeśli `engine Cast` się nie pojawi,
   sprawdzić, czy wiadomość czaru z EventManagera nie jest blokowana przez
   `BlocksForeignPlayerMagic` (Cast hook przepuszcza tylko
   `spell == gRequestedSpell`).
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

## Jak testować u użytkownika

1. Pobrać DLL z prerelease `dev013-latest` i sprawdzić `.sha256` oraz
   `tools/verify_dll.py`.
2. Podmienić `.parryhotter\build\ParryHotterHUD-G1.dll`.
3. Uruchomić `ZBUDUJ_MOD_PO_REPARSE.bat`, gdy zmieniły się skrypty, tekstury
   albo animacje (sama DLL tego nie wymaga).
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
