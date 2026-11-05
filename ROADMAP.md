# Parry Hotter — stan prac (Gothic I, prototyp DEV 0.10)

## Zrobione i przetestowane automatycznie

- [x] Sześć run (Flipendo, Levicorpus, Rictusempra, Expelliarmus, Bumbardo Maximus, Avada Kedavra) i podstawowe skrypty Daedalus.
- [x] Polski zestaw skryptów i OU.bin, automatyczne kopie zapasowe.
- [x] HUD Union: 6 slotów, symbole celu, środkowy celownik, maszyna stanów PPM + LPM.
- [x] Kompilacja testowej DLL Union dla Gothic I, 14 autorskich ikon TGA, testy gestów i kamery.
- [x] Prototyp kamery zza ramienia, sprawdzanie celu ze środka kamery, powrót do poprzednich ustawień.
- [x] Poprawiona konfiguracja myszy bez wymuszania globalnego Gothic2_Control=1.
- [x] Rictusempra i Avada Kedavra: zastępcze animacje Gothica i własne stany NPC.
- [x] Pobieranie/przygotowanie zasobów Harry'ego prywatnie, automatyczne wykrywanie ZIP w Downloads.
- [x] Przygotowanie tekstur HP2 PNG -> TGA przez Blendera (kod, wymaga testu na prawdziwych plikach).
- [x] Tymczasowa szata Maga Ognia zamiast stroju więźnia, bez importu zewnętrznych modeli.
- [x] Bezpieczna integracja **gotowego** lokalnego ASC jako strój Harry'ego: skrypty, MDS, tekstury, pakowanie tylko lokalnie.
- [x] Testy Python instalatora, źródeł PL, model assets, pakowania oraz testy kompilatora GDC.

## Niedokończone: konieczne do grywalnej wersji

- [ ] Import prawdziwego `skharrymesh.fbx`, szkieletu `HUM_BODY_NAKED0.ASC`, retarget w Blenderze, **eksport PH_HARRY.ASC** i test peleryny. Repo nie zawiera jeszcze gotowego modelu Gothica.
- [x] Model 3DS/TGA różdżki z oryginalnego lokalnego OBJ HP2 lub fallback generowany automatycznie, dołączony do lokalnego `.mod` (testy i kompilacja DLL).
- [ ] **Sprawdzić w grze** mocowanie różdżki do `ZS_RIGHTHAND` podczas PPM, pozycję i animacje; własne animacje ładowania niegotowe.
- [ ] Weryfikacja w rzeczywistym Gothic I i jego parserze oraz pakowanie przez GothicVDFS.exe użytkownika.
- [ ] Rzeczywiste efekty 6 zaklęć: lewitacja głową w dół, fizyka wybuchu/obrót w powietrzu, taniec, pełne rozbrojenie, niestandardowa animacja śmierci bez naruszania bazowego balansu.
- [x] Wstępne koszty many i osobne cooldowny 6 zaklęć, z przygaszaniem niedostępnych slotów i testami C++ (test w rzeczywistej grze wciąż potrzebny).
- [x] Prototyp Levicorpus: krótki, niedający obrażeń stan NPC, zamiast domyślnego 9-sekundowego zamrożenia (jeszcze bez uniesienia i obrotu 3D).
- [x] Zdarzenia trafień rozpoznają wybrany slot przez `PH_SELECTED_SPELL` (Union → Daedalus), a starsze haki są bezpiecznie aktualizowane bez dublowania.
- [x] Bumbardo: dodatkowy, krótki wstrząs człowieka przy trafieniu ponad natywny Stormfist (nie jest to jeszcze wyrzut z ragdollem).
- [x] Eksperymentalne impulsy po prawdziwym trafieniu: mały wybicie Levicorpus i mocniejsze wybicie Bumbardo, obliczenia z limitami, testy przeszkód i zgodności z aktywnym `zCRigidBody`.
- [x] `PH_UNION_LOADED` chroni zwykłe czary Gothica przed uruchomieniem specjalnych reakcji w profilu PLAY; builder wymaga nowego symbolu.
- [x] Eksperymentalny panel diagnostyczny pod **F10**: liczniki trafień, impulsów, lądowań oraz przyczyn braku fizyki, domyślnie ukryty (skompilowany i przetestowany automatycznie).
- [ ] **Test w silniku** czy NPC mają `physicsEnabled` / `GetRigidBody()` podczas trafienia. Panel F10 powinien ujawnić, gdzie blokuje się wyrzut; przy braku aktywnej fizyki dodatkowy impuls pozostaje pominięty.
- [x] Ograniczona czasowo obserwacja NPC po wybiciu, wykrywanie lądowania oraz jednorazowa próba animacji Bumbardo, z testami odbić i timeoutów (nadal bez testu w grze).
- [x] Rozpoznane trafienie jest diagnozowane najwyżej raz na rzut; przy otwarciu menu, cutscence lub zmianie świata obserwacja lotu jest przerywana i nie wykonuje animacji lądowania.
- [x] Zapis gry nie utrwala przypadkowo `PH_UNION_LOADED=1` dla profilu DEV bez pluginu.
- [ ] Peleryna, lewitacja głową w dół, pełne obroty NPC, pełny ragdoll i rzeczywiste testy animacji po lądowaniu.
- [x] Koniec globalnego osłabienia BreathOfDeath; stare kopie wartości służą do przywrócenia oryginału.
- [ ] Dźwięki, błyski, pozostałe animacje i usunięcie konfliktów 1–6 z bazowym interfejsem gry.
- [ ] Docelowe sterowanie swobodną kamerą z HP2 (obrót postaci i celowanie bez przeskoków).
- [ ] Jeden instalator wykonujący **również** bezobsługową kompilację ASC i .mod po legalnym pozyskaniu lokalnych źródeł.

**Bezpieczeństwo:** nie publikujemy plików skopiowanych z komercyjnych gier. Gotowy użytkowy model powstaje wyłącznie lokalnie; skrypty i oryginalne ikony HUD są autorskie.

**Najbliższy rzeczywisty blocker:** nie mamy dostępnego, wyeksportowanego Gothic-compatible `PH_HARRY.ASC` ani testu animacji w samej grze. Zielone testy GitHub Actions nie dowodzą działania w silniku ZenGin.
