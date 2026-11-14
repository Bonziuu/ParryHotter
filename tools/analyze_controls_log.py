#!/usr/bin/env python3
"""Summarize local gameplay logs without inferring unlogged success."""
from __future__ import annotations

import argparse
import json
import math
import re
import sys
from pathlib import Path

SPELL_NAMES = ("Flipendo", "Levicorpus", "Rictusempra", "Expelliarmus",
               "Bumbardo", "Avada")
COUNTS = ("requested", "confirmed", "refused", "timeout", "cancelled")
_TIMESTAMP = re.compile(r"^\s*(\d+(?:\.\d+)?)\s+")
_FIELDS = re.compile(r"\b([A-Za-z][A-Za-z0-9_]*)=([^\s]+)")
_NATIVE_POINTER = re.compile(r"(?:0[xX][0-9a-fA-F]{1,16}|[0-9a-fA-F]{8,16})\Z")
_BRACKET_FIELDS = re.compile(r"\b(name|texture)=\[([^\]\r\n]*)\]")


def _integer(value: str | None) -> int | None:
    try:
        return int(value) if value is not None else None
    except (ValueError, OverflowError):
        return None


def _nonnegative_float(value: str | None) -> float | None:
    try:
        number = float(value) if value is not None else None
    except (ValueError, OverflowError):
        return None
    return number if number is not None and math.isfinite(number) and number >= 0 else None


def _finite_float(value: str | None) -> float | None:
    try:
        number = float(value) if value is not None else None
    except (ValueError, OverflowError):
        return None
    return number if number is not None and math.isfinite(number) else None


def _native_pointer(value: str | None) -> int | None:
    """Read native %p output; None means unrecognised, zero means null.

    G1/MSVC prints eight hex digits without a prefix. Also accept 64-bit
    padding and the 0x/(nil) forms emitted by portable diagnostic runs.
    Do not interpret arbitrary words or decimal-looking short text as a vob.
    """
    if value is not None and value.lower() == "(nil)":
        return 0
    return int(value, 16) if value is not None and _NATIVE_POINTER.fullmatch(value) else None


def _stats(values: list[float]) -> dict:
    mean = sum(value / len(values) for value in values) if values else None
    if mean is not None and not math.isfinite(mean):
        mean = None
    return {"count": len(values), "min": min(values) if values else None,
            "max": max(values) if values else None,
            "mean": mean}


def analyze_log(text: str) -> dict:
    """Return counts and observed evidence; unknown/malformed lines are ignored.

    IDs deduplicate new-format requests/results. Legacy REQUEST lines without
    IDs are separate attempts. ``confirmed`` requires a successful engine Cast
    AND sameRequest=1. An engine result alone never proves projectile impact.
    """
    slots = [dict(slot=i, name=name, **dict.fromkeys(COUNTS, 0))
             for i, name in enumerate(SPELL_NAMES)]
    unassigned = dict.fromkeys(COUNTS, 0)
    build_markers: list[str] = []
    requests: dict[int, tuple[int | None, float | None]] = {}
    legacy_pending: dict[int, float | None] = {}
    seen: set[tuple[str, int]] = set()
    seen_inputs: set[int] = set()
    input_events: dict[str, int] = {}
    latencies: list[float] = []
    native_before = native_after = native_counter = input_dropped = 0
    successful_unmatched = malformed = total_lines = 0
    terminal_ids: set[int] = set()
    jump_phases = ("requested", "accepted", "refused", "landed", "interrupted", "expired")
    jump_counts = dict.fromkeys(jump_phases, 0)
    seen_jumps: set[tuple[int, str]] = set()
    jump_requested: set[int] = set()
    jump_accepted: set[int] = set()
    jump_terminal: set[int] = set()
    jump_reasons: dict[str, int] = {}
    jump_peaks: list[float] = []
    jump_ages: list[float] = []
    camera_distances: list[float] = []
    camera_pivots: list[float] = []
    camera_blocked = camera_roof = camera_events = 0
    movement_samples = movement_blocked = movement_events = low_progress = 0
    desired_speeds: list[float] = []
    actual_speeds: list[float] = []
    vertical_speeds: list[float] = []
    seen_aim_releases: set[int] = set()
    aim_releases = aim_acquired = aim_missed = aim_snapshots = 0
    aim_snapshot_acquired = aim_dimmed = aim_malformed = 0
    aim_modes = {"ray": 0, "assist": 0}
    aim_miss_reasons: dict[str, int] = {}
    seen_alpha_drops: set[tuple[float | None, str]] = set()
    render_alpha = {
        "observed": False,
        "guard_enabled": None,
        "guard_reason": None,
        "capacity": None,
        "peak_observed": None,
        "max_dropped_total_observed": None,
        "overflow_reports": 0,
        "bad_counter_samples_observed": None,
        "pfx_effects_peak": None,
        "particle_capacity": None,
        "last_drop_material": None,
        "last_drop_texture": None,
        "limitation": "Pomiary nie ustalaja przyczyny przepelnienia. Maksima licznikow nie sa sumowane; particle_capacity oznacza pojemnosc, nie aktywne czastki.",
    }

    def alpha_max(key: str, value: str | None, limit: int | None = None) -> None:
        number = _integer(value)
        if number is not None and number >= 0 and (limit is None or number <= limit):
            previous = render_alpha[key]
            render_alpha[key] = number if previous is None else max(previous, number)

    def count(category: str, slot: int | None) -> None:
        if slot is not None and 0 <= slot < len(slots):
            slots[slot][category] += 1
        else:
            unassigned[category] += 1

    for raw in text.splitlines():
        total_lines += 1
        raw = raw.lstrip("\ufeff")
        timestamp_match = _TIMESTAMP.match(raw)
        timestamp = _nonnegative_float(timestamp_match.group(1)) if timestamp_match else None
        body = raw[timestamp_match.end():] if timestamp_match else raw.strip()
        fields = dict(_FIELDS.findall(body))
        marker = re.match(r"build\s+(\S+)", body)
        if marker and marker.group(1) not in build_markers:
            build_markers.append(marker.group(1))
        native_before += int(body.startswith("magic before "))
        native_after += int(body.startswith("magic after "))
        calls = _integer(fields.get("magicCalls"))
        if calls is not None and calls >= 0:
            native_counter = max(native_counter, calls)
        for key in ("dropped", "inputDropped", "droppedEvents"):
            dropped = _integer(fields.get(key))
            if dropped is not None and dropped >= 0:
                # Snapshot values are cumulative, so repeated samples must
                # not inflate the number of lost input events.
                input_dropped = max(input_dropped, dropped)
        if body.startswith("input "):
            sequence = _integer(fields.get("seq"))
            kind = fields.get("kind")
            if kind and (sequence is None or sequence not in seen_inputs):
                input_events[kind] = input_events.get(kind, 0) + 1
                if sequence is not None:
                    seen_inputs.add(sequence)

        if body.startswith("alpha_guard ") or body.startswith("effects "):
            configuration = body.startswith("alpha_guard ") and "phase" not in fields
            alpha_record = configuration or fields.get("phase") in ("drop", "batch")
            enabled = _integer(fields.get("enabled")) if configuration else (
                _integer(fields.get("alphaGuard")) if body.startswith("effects ") else None)
            if enabled in (0, 1):
                if render_alpha["guard_enabled"] != bool(enabled):
                    render_alpha["guard_reason"] = None
                render_alpha["guard_enabled"] = bool(enabled)
                render_alpha["observed"] = True
                if configuration:
                    render_alpha["guard_reason"] = fields.get("reason")
            if body.startswith("effects "):
                alpha_max("pfx_effects_peak", fields.get("pfxEffects"))
                capacity = _integer(fields.get("particleCapacity"))
                if capacity is not None and capacity > 0:
                    render_alpha["particle_capacity"] = capacity
            if body.startswith("alpha_guard ") and alpha_record:
                capacity = _integer(fields.get("capacity"))
                if capacity is not None and capacity > 0:
                    render_alpha["capacity"] = capacity
            # A disabled/unsupported renderer never reads the raw counter.
            # Its zeroed policy snapshots must not become native measurements.
            if render_alpha["guard_enabled"] is not False:
                old_metrics = tuple(render_alpha[key] for key in (
                    "peak_observed", "max_dropped_total_observed", "bad_counter_samples_observed"))
                capacity = render_alpha["capacity"] or 2048
                if body.startswith("effects "):
                    alpha_max("peak_observed", fields.get("alphaPeak"), capacity)
                    alpha_max("max_dropped_total_observed", fields.get("alphaDroppedTotal"))
                elif alpha_record and fields.get("phase") in ("drop", "batch"):
                    alpha_max("peak_observed", fields.get("peak"), capacity)
                    alpha_max("peak_observed", fields.get("intervalPeak"), capacity)
                    alpha_max("max_dropped_total_observed", fields.get("droppedTotal"))
                    alpha_max("bad_counter_samples_observed", fields.get("badCounter"), 1)
                    alpha_max("bad_counter_samples_observed", fields.get("badCounterSamples"))
                    alpha_max("bad_counter_samples_observed", fields.get("intervalBadCounters"))
                    if fields["phase"] == "drop":
                        identity = (timestamp, body)
                        native = _integer(fields.get("native"))
                        dropped = _integer(fields.get("droppedTotal"))
                        bad = _integer(fields.get("badCounter"))
                        if (identity not in seen_alpha_drops and native == capacity and
                                dropped is not None and dropped > 0 and bad in (None, 0)):
                            seen_alpha_drops.add(identity)
                            render_alpha["overflow_reports"] += 1
                        # Copy complete bracketed names, including spaces.
                        names = dict(_BRACKET_FIELDS.findall(body))
                        if native is not None and bad in (None, 0, 1):
                            render_alpha["last_drop_material"] = names.get("name")
                            render_alpha["last_drop_texture"] = names.get("texture")
                new_metrics = tuple(render_alpha[key] for key in (
                    "peak_observed", "max_dropped_total_observed", "bad_counter_samples_observed"))
                if old_metrics != new_metrics or render_alpha["overflow_reports"]:
                    render_alpha["observed"] = True

        if body.startswith("aim "):
            phase = fields.get("phase")
            sequence = _integer(fields.get("seq"))
            mode = fields.get("mode")
            target = _native_pointer(fields.get("target"))
            reason = fields.get("reason")
            acquired = mode in aim_modes and target is not None and target > 0 and reason == "acquired"
            missed = mode == "none" and target == 0 and bool(reason) and reason != "acquired"
            # A partial/contradictory record is not evidence of either a
            # caught or missed target. Only valid releases reserve their ID.
            valid = (phase in ("release", "snapshot") and sequence is not None and
                     sequence >= (1 if phase == "release" else 0) and (acquired or missed))
            if not valid:
                aim_malformed += 1
            elif phase == "release":
                if sequence not in seen_aim_releases:
                    seen_aim_releases.add(sequence)
                    aim_releases += 1
                    if acquired:
                        aim_acquired += 1
                        aim_modes[mode] += 1
                    else:
                        aim_missed += 1
                        aim_miss_reasons[reason] = aim_miss_reasons.get(reason, 0) + 1
            else:
                # seq belongs to the last input, so successive 2 Hz samples
                # legitimately share it. Count samples rather than actions.
                aim_snapshots += 1
                if acquired:
                    aim_snapshot_acquired += 1
                    aim_dimmed += int(_integer(fields.get("ready")) == 0)

        if body.startswith("jump "):
            jump_id = _integer(fields.get("id"))
            phase = fields.get("phase")
            if jump_id is not None and jump_id > 0 and phase in jump_counts:
                identity = (jump_id, phase)
                if identity not in seen_jumps:
                    seen_jumps.add(identity)
                    jump_counts[phase] += 1
                    if phase == "requested":
                        jump_requested.add(jump_id)
                    if phase == "accepted":
                        jump_accepted.add(jump_id)
                    if phase in ("landed", "interrupted", "expired"):
                        jump_terminal.add(jump_id)
                    if phase in ("refused", "interrupted", "expired"):
                        reason = fields.get("reason", "unknown")
                        jump_reasons[reason] = jump_reasons.get(reason, 0) + 1
                    if phase == "landed":
                        peak = _nonnegative_float(fields.get("peakCm"))
                        age = _nonnegative_float(fields.get("ageSec"))
                        if peak is not None:
                            jump_peaks.append(peak)
                        if age is not None:
                            jump_ages.append(age)
        if body.startswith("controls "):
            distance = _nonnegative_float(fields.get("cameraDist"))
            pivot = _finite_float(fields.get("pivotHeight"))
            if distance is not None:
                camera_distances.append(distance)
                camera_blocked += int(_integer(fields.get("cameraBlocked")) == 1)
                camera_roof += int(_integer(fields.get("roofLimited")) == 1)
                if pivot is not None:
                    camera_pivots.append(pivot)
            velocity = [_finite_float(fields.get(name))
                        for name in ("desiredX", "desiredZ", "actualX", "actualZ")]
            if all(value is not None for value in velocity):
                dx, dz, ax, az = velocity
                desired = math.hypot(dx, dz)
                actual = math.hypot(ax, az)
                if math.isfinite(desired) and math.isfinite(actual):
                    movement_samples += 1
                    desired_speeds.append(desired)
                    actual_speeds.append(actual)
                    movement_blocked += int(_integer(fields.get("movementBlocked")) == 1)
                    # This is only an observed discrepancy, not a diagnosed wall
                    # collision: acceleration, braking or another action may act.
                    low_progress += int(desired >= 20 and actual < desired * 0.2)
            vertical = _finite_float(fields.get("vy"))
            if vertical is not None:
                vertical_speeds.append(vertical)
        if body.startswith("movement ") and _integer(fields.get("blocked")) == 1:
            movement_events += 1
        if body.startswith("camera ") and _integer(fields.get("blocked")) == 1:
            camera_events += 1

        category = None
        if body.startswith("cast "):
            if re.search(r"\bREQUEST\b", body):
                category = "requested"
            elif re.search(r"\bREFUSED\b", body):
                category = "refused"
            elif re.search(r"\bTIMEOUT\b", body):
                category = "timeout"
            elif re.search(r"\b(?:CANCEL|CANCELLED|CANCELED)\b", body):
                category = "cancelled"
        engine_cast = body.startswith("engine Cast ")
        if category is None and not engine_cast:
            continue

        request_id = _integer(fields.get("request"))
        if request_id is not None and request_id <= 0:
            request_id = None
        slot = _integer(fields.get("slot"))
        if slot is None and request_id in requests:
            slot = requests[request_id][0]
        if engine_cast:
            result = _integer(fields.get("result"))
            same = _integer(fields.get("sameRequest"))
            if result is None:
                malformed += 1
                continue
            if result != 0 and same != 1:
                key = ("unmatched", request_id)
                if request_id is None or key not in seen:
                    successful_unmatched += 1
                    if request_id is not None:
                        seen.add(key)
                continue
            if result == 0:
                category = "refused"
            else:
                category = "confirmed"

        assert category is not None
        if request_id is not None:
            identity = (category, request_id)
            if identity in seen:
                continue
            seen.add(identity)
        # Slots 6..9 are valid empty HUD slots: keep their refusals outside
        # the six implemented spells, without calling those lines malformed.
        if slot is None or not 0 <= slot < 10:
            malformed += 1
        count(category, slot)
        if category == "requested":
            if request_id is not None:
                requests[request_id] = (slot, timestamp)
            elif slot is not None:
                legacy_pending[slot] = timestamp
        elif request_id is not None:
            terminal_ids.add(request_id)
        if category == "confirmed":
            latency = _nonnegative_float(fields.get("latencyMs"))
            if request_id is not None:
                started = requests.get(request_id, (None, None))[1]
            else:
                started = legacy_pending.pop(slot, None)
            if latency is None and started is not None and timestamp is not None and timestamp >= started:
                latency = (timestamp - started) * 1000
            if latency is not None:
                latencies.append(latency)

    totals = {category: sum(slot[category] for slot in slots) + unassigned[category]
              for category in COUNTS}
    native_observed = native_before > 0 or native_after > 0 or native_counter > 0
    warnings = []
    if totals["requested"] and not native_observed:
        warnings.append("Brak zarejestrowanego MagicMode przy zadaniach rzutu; sprawdz tryb broni i diagnostyke AI.")
    if totals["requested"] and not totals["confirmed"]:
        warnings.append("Log nie potwierdza ani jednego udanego rzutu z dopasowanym zadaniem.")
    if input_dropped:
        warnings.append("Kolejka wejscia utracila zdarzenia; sprawdz wpisy przerwania i przeciazenia.")
    if successful_unmatched:
        warnings.append("Sa sukcesy engine Cast bez sameRequest=1; nie zaliczono ich do potwierdzonych zadan.")
    if not build_markers:
        warnings.append("Brak znacznika wersji DLL; log moze byc starszy albo niekompletny.")
    if malformed:
        warnings.append("Czesc wpisow ma brakujace lub nieprawidlowe pola; zobacz unattributed i malformed_lines.")
    if aim_malformed:
        warnings.append("Pominieto niepelne lub sprzeczne wpisy namierzania; zobacz aim.malformed_samples.")
    return {
        "build_markers": build_markers,
        "slots": slots,
        "totals": totals,
        "unattributed": unassigned,
        "native_magic_observed": native_observed,
        "missing_native_magic": bool(totals["requested"] and not native_observed),
        "native_magic_before": native_before,
        "native_magic_after": native_after,
        "native_magic_counter_max": native_counter,
        "input_events": input_events,
        "input_dropped": input_dropped,
        "successful_unmatched": successful_unmatched,
        "pending_request_ids": sorted(set(requests) - terminal_ids),
        "latency_ms": _stats(latencies),
        "render_alpha": render_alpha,
        "aim": {
            "observed": bool(aim_releases or aim_snapshots),
            "release_samples": aim_releases,
            "acquired_releases": aim_acquired,
            "missed_releases": aim_missed,
            "release_modes": aim_modes,
            "miss_reasons": aim_miss_reasons,
            "snapshot_samples": aim_snapshots,
            "acquired_snapshots": aim_snapshot_acquired,
            "dimmed_snapshots": aim_dimmed,
            "malformed_samples": aim_malformed,
            "limitation": "Namierzenie nie dowodzi rzutu ani trafienia; brak probek oznacza brak pomiaru.",
        },
        "jumps": {
            "phases": jump_counts,
            "reasons": jump_reasons,
            "pending_ids": sorted(jump_requested - jump_terminal),
            "active_ids": sorted(jump_accepted - jump_terminal),
            "landed_peak_cm": _stats(jump_peaks),
            "landed_airtime_sec": _stats(jump_ages),
            "limitation": "Odmowa proby moze poprzedzac przyjecie tego samego skoku; pomiary nie dowodza zgodnosci z HP2.",
        },
        "camera": {
            "samples": len(camera_distances),
            "blocked_samples": camera_blocked,
            "roof_limited_samples": camera_roof,
            "blocked_event_lines": camera_events,
            "distance_cm": _stats(camera_distances),
            "pivot_above_head_cm": _stats(camera_pivots),
            "limitation": "Probki 2 Hz nie dowodza braku przenikania pomiedzy probkami; potrzebna jest obserwacja obrazu.",
        },
        "movement": {
            "samples": movement_samples,
            "blocked_samples": movement_blocked,
            "blocked_event_lines": movement_events,
            "low_progress_samples": low_progress,
            "desired_speed_cm_s": _stats(desired_speeds),
            "actual_speed_cm_s": _stats(actual_speeds),
            "vertical_speed_cm_s": _stats(vertical_speeds),
            "limitation": "Maly postep sam w sobie nie rozstrzyga, czy przyczyna jest kolizja, hamowanie lub inna akcja.",
        },
        "malformed_lines": malformed,
        "total_lines": total_lines,
        "warnings": warnings,
        "limitation": "Potwierdzony engine Cast nie dowodzi trafienia ani zgodnosci sterowania z HP2.",
    }


def analyze_file(path: Path) -> dict:
    return analyze_log(Path(path).read_text(encoding="utf-8-sig", errors="replace"))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Podsumuj lokalny log testow sterowania Parry Hotter")
    parser.add_argument("log", type=Path, help="ParryHotter_cast.log")
    parser.add_argument("--json", action="store_true", help="Wypisz raport JSON")
    args = parser.parse_args(argv)
    try:
        report = analyze_file(args.log)
    except OSError as error:
        parser.exit(1, f"BLAD: {error}\n")
    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
        return 0
    print("Wersja DLL: " + (", ".join(report["build_markers"]) or "brak w logu"))
    print("Slot | Czar | Zadania | Potwierdzone | Odmowy | Timeout | Anulowane")
    for slot in report["slots"]:
        print(f"{slot['slot'] + 1} | {slot['name']} | {slot['requested']} | "
              f"{slot['confirmed']} | {slot['refused']} | {slot['timeout']} | {slot['cancelled']}")
    print(f"Zarejestrowano MagicMode: {'tak' if report['native_magic_observed'] else 'nie'}")
    print(f"Utracone zdarzenia wejscia: {report['input_dropped']}")
    latency = report["latency_ms"]
    if latency["count"]:
        print(f"Czas release/request do Cast: srednio {latency['mean']:.1f} ms "
              f"(min {latency['min']:.1f}, max {latency['max']:.1f})")
    jumps = report["jumps"]
    print("Skoki: " + ", ".join(f"{phase}={count}" for phase, count in jumps["phases"].items()))
    if jumps["landed_peak_cm"]["count"]:
        print(f"Zmierzona wysokosc zakonczonych skokow: srednio {jumps['landed_peak_cm']['mean']:.1f} cm")
    if jumps["landed_airtime_sec"]["count"]:
        print(f"Zmierzony czas zakonczonych skokow: srednio {jumps['landed_airtime_sec']['mean']:.3f} s")
    camera = report["camera"]
    print(f"Kamera: probki={camera['samples']}, ograniczone={camera['blocked_samples']}, dach={camera['roof_limited_samples']}")
    movement = report["movement"]
    print(f"Ruch: probki={movement['samples']}, zablokowane={movement['blocked_samples']}, maly_postep={movement['low_progress_samples']}")
    aim = report["aim"]
    if aim["observed"]:
        print(f"Namierzanie przy puszczeniu LPM: namierzone={aim['acquired_releases']}, "
              f"bez_celu={aim['missed_releases']}, promien={aim['release_modes']['ray']}, "
              f"pomoc={aim['release_modes']['assist']}")
        print(f"Probki namierzania: {aim['snapshot_samples']}, "
              f"namierzone_z_niegotowym_czarem={aim['dimmed_snapshots']}")
        if aim["miss_reasons"]:
            print("Przyczyny braku celu: " + ", ".join(
                f"{reason}={count}" for reason, count in sorted(aim["miss_reasons"].items())))
    else:
        print("Namierzanie: brak prawidlowych pomiarow w logu")
    alpha = report["render_alpha"]
    if not alpha["observed"]:
        print("Renderer alpha: brak pomiarow w logu")
    else:
        enabled = {True: "aktywna", False: "wylaczona", None: "brak informacji"}[alpha["guard_enabled"]]
        print(f"Renderer alpha: ochrona={enabled}, powod={alpha['guard_reason'] or 'brak informacji'}")
        print(f"Zmierzony szczyt puli alpha: {alpha['peak_observed'] if alpha['peak_observed'] is not None else 'brak pomiaru'}, "
              f"maksymalny zaobserwowany droppedTotal={alpha['max_dropped_total_observed'] if alpha['max_dropped_total_observed'] is not None else 'brak pomiaru'}, "
              f"raporty przepelnienia={alpha['overflow_reports']}")
    if alpha["pfx_effects_peak"] is not None or alpha["particle_capacity"] is not None:
        print(f"PFX: najwiecej wpisow={alpha['pfx_effects_peak'] if alpha['pfx_effects_peak'] is not None else 'brak pomiaru'}, "
              f"pojemnosc puli czastek={alpha['particle_capacity'] if alpha['particle_capacity'] is not None else 'brak pomiaru'}")
    for warning in report["warnings"]:
        print("Uwaga: " + warning)
    print(report["limitation"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
