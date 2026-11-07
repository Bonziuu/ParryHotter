# Model Harry'ego — pliki do scalenia w Claude Code

Przygotowany model jest dołączony do gałęzi `codex/dev013-handoff` jako
[ParryHotter_Harry_Gothic.zip](../ParryHotter_Harry_Gothic.zip). Zwykłe pobranie
tej gałęzi przez Git pobiera również paczkę; plik nie korzysta z Git LFS.
Instalator wykrywa ją automatycznie w katalogu głównym projektu.

## Dostęp do plików

Przy istniejącym klonie pobierz aktualną gałąź:

```bash
git fetch origin
git switch codex/dev013-handoff
git pull --ff-only
```

Jeśli scalasz projekt na innej gałęzi, zachowaj tamtejsze zmiany i scal
`origin/codex/dev013-handoff`. Kompletne wymagania i dalsze etapy są w
[PROMPT_DO_NOWEGO_CZATU.md](../PROMPT_DO_NOWEGO_CZATU.md).

Suma SHA-256 paczki:

```text
f0378e38ee1935ac7b7f420c592e00d464e7cfc7d91db6c7124bdd14b65b72f9
```

Kontrola sumy na Linux/macOS z `sha256sum`:

```bash
sha256sum -c ParryHotter_Harry_Gothic.zip.sha256
```

Na Windows:

```powershell
Get-FileHash ParryHotter_Harry_Gothic.zip -Algorithm SHA256
```

Kontrola struktury modelu, tekstur i animacji bez modyfikowania gry:

```bash
python -c "from pathlib import Path; from tools.import_harry_bundle import validate_harry_bundle; validate_harry_bundle(Path('ParryHotter_Harry_Gothic.zip')); print('Model, tekstury i animacje: OK')"
```

## Zawartość paczki

Osiem gotowych plików źródłowych do importera:

- `PH_HARRY.ASC` — rzeczywisty Harry HP2 PC na 25 kościach Gothica;
- `PH_HARRY_TEX0.TGA`–`PH_HARRY_TEX3.TGA` — cztery tekstury;
- `PH_HARRY_CAST.MDS` — nakładka animacji wyłącznie dla bohatera;
- `PH_HARRY_CHARGE.ASC` — przygotowanie/pętla czarowania, 43 klatki;
- `PH_HARRY_RELEASE.ASC` — rzut/powrót, 26 klatek.

Model ma 710 ważonych wierzchołków i 1016 trójkątów. Klipy mają 30 FPS.
Archiwum zawiera także projekty Blendera, w tym `ParryHotter_CASTING.blend`,
podglądy i raporty walidacji. Nazwy i sumy zasobów podaje `MANIFEST.json`.
Informację o pochodzeniu oryginalnego modelu zachowano w
`INFOmesh_original.txt` i manifeście. Dołączone pliki nie oznaczają przeniesienia
praw do oryginalnych zasobów.

Do pracy nad geometrią lub klipami rozpakuj paczkę do lokalnego katalogu:

```bash
python -m zipfile -e ParryHotter_Harry_Gothic.zip .parryhotter/harry-bundle
```

## Instalacja do źródeł Gothica

Wymagane są Gothic I, Union i źródła ModKitu, w szczególności `Gothic.src`,
`PC_Hero.d` i `HUMANS.MDS`. Podaj właściwą ścieżkę gry:

```powershell
py -3 tools/setup.py --game "C:\Gry\Gothic"
```

Sam import przygotowanego modelu do istniejących źródeł:

```powershell
py -3 -m tools.import_harry_bundle --game "C:\Gry\Gothic" --bundle "ParryHotter_Harry_Gothic.zip"
```

Na Linux odpowiednikiem polecenia `py -3` jest `python3`. Import waliduje dane,
tworzy kopie i rejestruje model oraz nakładkę w źródłach. Gotowa paczka nie
wymaga ponownego pobierania modelu ani uruchamiania Blendera do tego importu.

## Stan końcowego uruchomienia

Model i oba klipy przeszły rzeczywisty eksport i ponowny import w Blenderze oraz
instalację na źródłach Unified-PL/ModKitu. Raporty w ZIP opisują moment konwersji;
późniejszy, potwierdzony build DLL opisuje [DEV-013.md](DEV-013.md).

Pozostają kompilacja modeli/tekstur/MSB/MAN przez Gothic, **Reparse Scripts**,
**nowa gra** dla konstruktora `PC_Hero`, budowa lokalnego `.mod` i test ZenGin.
Własne klipy nie zawierają chodu nóg podczas celowania. Pełny ragdoll i odwrócona
lewitacja także pozostają do dokończenia zgodnie z promptem przekazania.
