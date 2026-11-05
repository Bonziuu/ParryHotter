# Roadmap — Parry Hotter

## v0.1 — skrypty prototypu (obecny etap)

- [x] Repozytorium i instrukcja uruchomienia.
- [x] Sześć run z istniejącymi efektami Gothic 1.
- [x] Patch lokalnego PC_Hero: Parry Hotter, 200 many, 6 krąg, wyposażenie startowe.
- [x] Próba rozbrojenia celem Expelliarmus (AI_DropItem).
- [x] Stan zabawnej śmierci po Avada Kedavra (animacja porażenia, po 2 s obrażenia śmiertelne).
- [x] Testy instalatora.
- [ ] **Weryfikacja kompilacji i działania w Gothic 1 na Windows**. Obecny kod to kandydat do pierwszych testów, nie wydany gotowy mod.
- [ ] Spakowanie skompilowanych skryptów do pliku .mod z GothicVDFS i publikacja wydania.

## v0.2 — mysz, celowanie i animacje

- [ ] Projekt wtyczki **Union C++ dla Gothic 1**: kliknięcie LPM wywołuje czar, PPM celowanie, rolka zmienia aktywną runę.
- [ ] Nakładka HUD z nazwą aktywnego zaklęcia i celownikiem.
- [ ] Zaklęcia bez potrzeby wybierania runy w ekwipunku; koszt many i cooldown.
- [ ] Sterowany odrzut i unoszenie NPC (logika w silniku, zabezpieczenia przed upadkiem przez mapę).
- [ ] Efekty wizualne i dźwiękowe z własnych/licencjonowanych zasobów.

## v0.3 — Parry Hotter i fizyka komediowa

- [ ] Zastąpienie modelu testowego własnym modelem Parry'ego i różdżką.
- [ ] Eksperymentalne użycie modelu Harry'ego z Komnaty Tajemnic **tylko lokalnie**, z zasobów legalnie posiadanej gry.
- [ ] Animacje: podrzucanie, zawieszanie głową w dół, wirowanie, salta, śmieszne zgony.
- [ ] Wiele losowych sekwencji Avada Kedavra; specjalny rzadki efekt.
- [ ] Testy kompatybilności zapisów i kolizji w trudnych miejscach (Stary Obóz, kopalnie, orkowie).

## Ograniczenia i bezpieczeństwo

Nie dołączamy oryginalnych modeli, tekstur, muzyki, animacji ani skryptów z żadnej komercyjnej gry. Główne zadanie v0.1 to weryfikacja Daedalusa i podstawowych efektów; Gothic 1 nie udostępnia natywnej fizyki ragdoll.
