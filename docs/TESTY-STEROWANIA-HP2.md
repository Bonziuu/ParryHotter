# Testy sterowania Parry Hotter — profil HP2 PC

Ta instrukcja sprawdza zachowanie moda w Gothic I Classic 1.08k z Union.
Testy automatyczne sprawdzają logikę i adaptery na modelach silnika. Nie
potwierdzają samodzielnie, że natywna DLL poprawnie wykonuje akcje w grze.

W tej iteracji przygotowano kolejkę naciśnięć/puszczeń, rozdzielenie wyboru
slotu od przygotowanego czaru oraz nowe moduły kamery, skoku i diagnostyki.
Zintegrowana DLL zapisuje znacznik `PH_CAST_V5_BODY_AIM_AVADA_READY` oraz
odpowiednie wpisy logu. Brak pola pomiarowego oznacza **brak pomiaru**,
a nie pozytywny wynik testu.

Niski skok ma autorski parametr docelowy **70 cm** przy natywnej grawitacji
Gothica (981 cm/s²); na równej ziemi daje to teoretycznie około **0,76 s** lotu.
To ustawienie do oceny w grze, nie potwierdzony pomiar HP2. Proceduralny cykl
nóg podczas celowania również jest autorski; nie jest importem klipów HP2.
Zgodność odczucia sterowania 1:1 wymaga porównania z HP2 PC.

## Przygotowanie próby

1. Zamknij Gothic i uruchom `AKTUALIZUJ_PARRY_HOTTER.bat` z lokalnego projektu.
   Aktualizator sprawdzi SHA-256 i zachowa kopie podmienianych plików.
2. W GothicStarter wybierz **Parry Hotter (PLAY)**. Użyj testowego zapisu lub
   nowej gry, bez ryzyka utraty postępu. Do oceny zużycia many wyłącz jej
   nieskończony zapas. Zapewnij żywy cel i wystarczającą manę dla każdej próby.
3. Po rozpoczęciu próby nie uruchamiaj ponownie gry, dopóki nie skopiujesz
   `Gothic\ParryHotter_cast.log`; nowy start nadpisuje bieżący log.
4. Zapisz kolejność prób i przybliżone sekundy nagrania. Przy przesyłaniu filmu
   do tej sesji przygotuj krótkie fragmenty o rozmiarze najwyżej 32 MiB.

Modele, tekstury i pozostałe oryginalne zasoby pozostają lokalne. Nie dodawaj
ich ani lokalnie zbudowanego `.mod` do repozytorium lub paczki narzędzi.

## Zaklęcia i wejście myszy

Najpierw wybierz Flipendo i rzuć **trzy razy pod rząd bez zmiany slotu**.
Przy każdym LPM różdżka ma przechodzić do celowania bez wyciągania broni.
Puszczenie na celu powinno rozpocząć rzut; schowanie nie może go zastąpić.
Log powinien pokazać trzy REQUEST i trzy udane `engine Cast`; pomiędzy nimi
`lastCast=1` ma zostać wyczyszczone przez sekwencję `rearm=1`, ACTION=1/UP=0.
Powtórz dla pozostałych pięciu czarów.


| Próba | Wykonanie | Wynik do potwierdzenia w grze |
| --- | --- | --- |
| Każdy z sześciu czarów | Wybierz kolejno 1–6. Celuj LPM w żywego NPC do 19 m, następnie puść. | Jeden rzut na puszczenie; efekt dotyczy wybranego celu. Log ma REQUEST i udany `engine Cast` z tym samym zadaniem. |
| Bardzo krótki LPM | Naciśnij i puść LPM szybko, stojąc na wprost celu; powtórz przy około 30 FPS. | Pełne kliknięcie nie ginie między klatkami. Zdarzenia press i release mają różne sekwencje; przygotowanie może zakończyć się po puszczeniu. |
| Długie LPM | Trzymaj LPM kilka sekund na celu, następnie puść. | Brak samoczynnego pocisku podczas trzymania. Puszczenie wywołuje najwyżej jeden rzut. |
| Bez celu | Celuj w pustą przestrzeń i puść LPM. Powtórz po zejściu kropką z NPC. | Brak wystrzału. Anulowanie nie zostawia stale wciśniętych natywnych ACTION/UP. Zużycie many ocenić oddzielnie. |
| Rzut podczas ruchu | Trzymaj W albo A/D, celuj i puść LPM na NPC. | Ruch jest płynny, WASD nie obraca postaci samoistnie, native walking nie blokuje rzutu. Nogi nie pozostają sztywne przy przesuwaniu. |
| Slot i puszczenie | Przygotuj 1, zmień na 2 dokładnie przy puszczaniu LPM. Potem zmień slot podczas dłuższego trzymania. | Rzut ma jednoznaczny przygotowany slot; nie ma odmowy wynikającej z pomieszania ID. Wybór pustego 7–0 anuluje, nie rzuca. |
| Zmiana celu | Ustaw dwóch NPC blisko siebie. Przesuwaj lekko mysz, potem wyraźnie przejdź na drugiego NPC. | Symbol nie migocze przy drobnym drgnięciu. Po puszczeniu czar nie przeskakuje sam na sąsiada. |
| Cel przestaje być poprawny | Puść na NPC, który za chwilę umrze, przejdzie za ścianę albo poza 19 m. | Oczekujące żądanie jest prawidłowo anulowane/odrzucone; brak trafienia przez przeszkodę i brak wiszącego celu po zmianie świata. |

Sloty w logu są liczone od zera: 0 Flipendo, 1 Levicorpus, 2 Rictusempra,
3 Expelliarmus, 4 Bumbardo, 5 Avada. Klawisze 7–0 to puste sloty.
Udany `engine Cast` dowodzi wykonania natywnego rzutu, ale nie dowodzi trafienia:
efekt na NPC trzeba dodatkowo obejrzeć.

## Przechwytywanie celu V5

Przytrzymaj LPM i prowadź kropkę przez głowę, nogi oraz tuż obok ciała NPC.
Sprawdź małego wroga blisko i człowieka dalej. Znak ma pojawiać się bez
konieczności trafienia w jeden punkt tułowia. Przy dwóch NPC przejdź wyraźnie
kropką na drugiego: znak powinien zmienić cel. Stary cel nie powinien
blokować nowego.

Powtórz przy ścianie, za przeszkodą i poza 19 m: pomoc nie może omijać
widoczności ani zasięgu. Przy odnowieniu lub braku many znak pozostaje
przygaszony (cel przechwycony, czar niedostępny).

Avadę sprawdź kilkoma krótkimi kliknięciami na żywym celu, z przerwą na
odnowienie. Puszczenie zbyt wcześnie powinno dokończyć konieczne ładowanie
i rzucić, zamiast kończyć się `engine_stop`. Osobno skocz podczas shoot:
przerwanie powinno być jawne, bez ponownego zużycia pełnej many dla tej
samej sekwencji puszczenia.

Nowe logi: `aim phase=release` opisuje dokładny moment puszczenia LPM,
a `phase=snapshot` aktualny stan 2 Hz. `mode=ray|assist|none`, `reason`,
`angle`, `eligible`, `tested`, `cameraBlocked`, `actorBlocked` wyjaśniają
przechwycenie lub brak celu. `angle` to kąt od rzutowanej bryły, nie od
jednego punktu; 0 oznacza kropkę na obszarze bryły. `ready=0` w próbce
oznacza niedostępność czaru i przygaszony znak, a nie brak celu.

## Konsola, menu i przerwania

Otwórz konsolę Gothica, wpisz tekst zawierający W/A/S/D, cyfry i spację.
Porusz myszą, sprawdź LPM/PPM oraz F10/F11. Postać, kamera i wybór slotu moda
nie powinny reagować na obsługę konsoli. Zamknij konsolę z wciąż trzymanym
przyciskiem myszy: nie powinien samoczynnie wystąpić rzut ani skok. Następne
zwykłe naciśnięcie po puszczeniu powinno działać.

Powtórz przerwanie przez ekwipunek, menu, rozmowę oraz zapis/wczytanie.
Sprawdź też wczytanie innego świata i powrót z wody. Log powinien zawierać
przyczynę zmiany kontekstu, a po powrocie nowe sekwencje wejścia.

## Skok i ruch

1. Na równej ziemi kliknij raz PPM bez WASD, potem z W, S i A/D. Oczekuj
   pojedynczego niskiego skoku i kontroli poziomego kierunku w powietrzu.
2. Przytrzymaj PPM przez cały lot i lądowanie. Nie powinien pojawić się drugi
   skok. Puść i kliknij ponownie — kolejny skok powinien być możliwy.
3. Powtórz krótki klik przy niskim FPS oraz naciśnięcie tuż przed lądowaniem.
   Bufor ma krótki czas ważności; nie powinien uruchamiać odłożonego skoku po
   dłuższej przerwie albo po zamknięciu menu.
4. Skocz pod niskim sufitem, przy ścianie, pod schodami i przy krawędzi.
   Kolizja może skrócić wysokość/lot. Nie powinno być drugiego impulsu,
   wspinania wymuszonego samym PPM ani przenikania przez sufit.
5. Zejdź z krawędzi podczas LPM. Opadanie powinno zachowywać natywną grawitację
   i kolizję, bez wiszenia w pozie czaru lub skokowej korekty pozycji w dół.
6. Napieraj na ścianę i przesuwaj się wzdłuż niej do narożnika. Po puszczeniu
   WASD nie powinien pozostawać zapamiętany rozpęd ani odrzut od narożnika.

## Kamera

Przetestuj otwartą przestrzeń, plecy przy ścianie, narożnik, drzwi oraz niski
dach. Obracaj kamerę poziomo i pionowo, stojąc oraz biegnąc. Sprawdź szeroki
obraz i standardowe proporcje ekranu, jeśli masz taką możliwość.

Kamera powinna natychmiast zbliżać się przy przeszkodzie, obniżać punkt nad
głową pod dachem i łagodnie wracać po wyjściu na otwartą przestrzeń. Nie powinna
przecinać ściany ani gubić Harry'ego poza kadrem. Kropka pozostaje na środku.
Logi kamery opisują obliczenia kolizji; brak migania/przenikania trzeba sprawdzić
na obrazie także między próbkami diagnostyki.

## Format diagnostyki i automatyczne podsumowanie

Wejście: `input seq=... kind=LeftPressed|LeftReleased|RightPressed|RightReleased|SlotSelected|Interrupted slot=... target=...`.
Żądania rzutu mają `request` i `seq`; `engine Cast` podaje wynik, `sameRequest`
oraz `latencyMs`. `REFUSED`, `CANCEL`, `CANCELLED` i `TIMEOUT` oznaczają osobne
obserwacje; nie należy zaliczać ich jako udanego wystrzału.

Format zdarzeń skoku:

```text
jump id=1 seq=20 phase=requested reason=press held=1 physics=0 vy=0 floor=0 ageSec=0 peakCm=0 targetHeightCm=70
jump id=1 seq=20 phase=accepted reason=accepted held=1 physics=1 vy=370.6 floor=0 ageSec=0 peakCm=0 targetHeightCm=70
jump id=1 seq=20 phase=landed reason=landed held=1 physics=0 vy=0 floor=0 ageSec=0.76 peakCm=69.8 targetHeightCm=70
```

Fazy to `requested`, `accepted`, `refused`, `landed`, `interrupted`, `expired`.
`refused` może być nieudaną próbą przed późniejszym `accepted` tego samego
żądania. Statystyki pełnego lotu obejmują wyłącznie `landed`; `ageSec` to sekundy,
`peakCm` to wysokość nad punktem startu. Identyfikator skoku jest monotoniczny.

Próbka `controls` może zawierać `desiredX`, `desiredZ`, `actualX`, `actualZ`,
`vy` w cm/s oraz `cameraDist`, `cameraWant`, `pivotHeight` w cm. Flagi
`movementBlocked`, `cameraBlocked`, `roofLimited` mają wartość 0 lub 1.
Krótkie zdarzenia mogą używać `movement seq=... blocked=1 reason=collision`
i `camera seq=... blocked=1 roofLimited=1`. Stałe próbki mają najwyżej 2 Hz;
ważne przejścia powinny być zapisane osobno. Sam mały postęp ruchu nie rozstrzyga
przyczyny i nie jest automatycznie diagnozą kolizji.

```powershell
py -3 -m tools.analyze_controls_log "C:\Programy\Steam\steamapps\common\Gothic\ParryHotter_cast.log"
py -3 -m tools.analyze_controls_log "C:\Programy\Steam\steamapps\common\Gothic\ParryHotter_cast.log" --json
```

Analizator uruchamiaj z aktualnego folderu projektu zawierającego to narzędzie.
Wynik bez próbek skoku lub kamery oznacza, że tych pomiarów nie znaleziono.
Przekaż log wraz z opisem nieudanej próby i krótkim nagraniem; bez filmu nie
da się potwierdzić wyglądu animacji, kadru ani zgodności wizualnej z HP2.
