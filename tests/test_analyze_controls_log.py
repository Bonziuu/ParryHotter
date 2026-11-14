"""Reports describe recorded evidence, never assumed successful gameplay."""
from contextlib import redirect_stdout, redirect_stderr
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from tools.analyze_controls_log import analyze_file, analyze_log, main


class ControlsLogAnalysisTests(unittest.TestCase):
    def test_new_trace_links_requests_results_and_unassigned_cancel(self):
        trace = """0.000 build PH_CAST_CONTROLS_V3
1.000 input seq=10 kind=LeftPressed slot=0 target=no
1.250 input seq=11 kind=LeftReleased slot=0 target=00ABCDEF
1.250 cast request=7 seq=11 slot=0 REQUEST target=00ABCDEF
1.300 magic before spellId=9 status=1 invested=0
1.350 player AI after npcMode=7 magicCalls=2
1.600 engine Cast slot=0 result=1 sameRequest=1 request=7 latencyMs=350.0
2.000 cast request=8 seq=12 slot=1 REQUEST target=ABC
2.100 cast request=8 CANCEL reason=target_dead
2.500 controls seq=12 request=0 slot=1 context=available inputDropped=4
3.000 controls seq=12 request=0 slot=1 context=available inputDropped=4
"""
        report = analyze_log(trace)
        self.assertEqual(report["totals"], dict(requested=2, confirmed=1, refused=0, timeout=0, cancelled=1))
        self.assertEqual(report["slots"][1]["cancelled"], 1)
        self.assertTrue(report["native_magic_observed"])
        self.assertFalse(report["missing_native_magic"])
        self.assertEqual(report["native_magic_counter_max"], 2)
        self.assertEqual(report["input_dropped"], 4)
        self.assertEqual(report["input_events"], {"LeftPressed": 1, "LeftReleased": 1})
        self.assertEqual(report["latency_ms"], dict(count=1, min=350.0, max=350.0, mean=350.0))
        self.assertEqual(report["pending_request_ids"], [])

    def test_repeated_ids_and_input_sequences_are_not_counted_twice(self):
        line = "1.000 cast request=4 seq=7 slot=4 REQUEST target=ABC\n"
        result = "1.400 engine Cast slot=4 result=1 sameRequest=1 request=4\n"
        event = "0.500 input seq=7 kind=LeftReleased slot=4 target=ABC\n"
        report = analyze_log(line + line + event + event + result + result)
        self.assertEqual(report["slots"][4]["requested"], 1)
        self.assertEqual(report["slots"][4]["confirmed"], 1)
        self.assertEqual(report["input_events"]["LeftReleased"], 1)
        self.assertAlmostEqual(report["latency_ms"]["mean"], 400.0)

    def test_legacy_windows_log_preserves_separate_requests_and_cancellations(self):
        trace = ("\ufeff0.000 build PH_CAST_INPUT_NATIVE_READY_V2\r\n"
                 "16.801 cast slot=0 REQUEST target=ABC status=1 invested=0\r\n"
                 "20.801 cast slot=0 TIMEOUT status=1 invested=0\r\n"
                 "30.000 cast slot=0 REQUEST target=ABC status=1 invested=0\r\n"
                 "34.000 cast slot=0 TIMEOUT status=1 invested=0\r\n"
                 "40.000 cast slot=2 REQUEST target=ABC status=0 invested=0\r\n"
                 "40.023 cast slot=2 CANCELLED by engine status=1 invested=-1\r\n"
                 "41.000 cast slot=1 REFUSED spell=1 validTarget=0\r\n")
        report = analyze_log(trace)
        self.assertEqual(report["totals"], dict(requested=3, confirmed=0, refused=1, timeout=2, cancelled=1))
        self.assertEqual(report["slots"][0]["requested"], 2)
        self.assertTrue(report["missing_native_magic"])
        self.assertEqual(report["build_markers"], ["PH_CAST_INPUT_NATIVE_READY_V2"])
        self.assertTrue(any("nie potwierdza" in text for text in report["warnings"]))

    def test_success_without_matching_request_is_not_confirmed(self):
        report = analyze_log("1.0 engine Cast slot=0 result=1 sameRequest=0 request=9\n"
                             "1.1 engine Cast slot=0 result=1 request=10\n"
                             "2.0 engine Cast slot=0 result=0 sameRequest=1 request=11\n")
        self.assertEqual(report["totals"]["confirmed"], 0)
        self.assertEqual(report["totals"]["refused"], 1)
        self.assertEqual(report["successful_unmatched"], 2)
        self.assertEqual(report["latency_ms"]["count"], 0)

    def test_unknown_and_malformed_lines_do_not_invent_slot_or_latency(self):
        report = analyze_log("random text\n"
                             "1.0 cast slot=nan REQUEST target=ABC\n"
                             "2.0 cast slot=99 REFUSED reason=empty\n"
                             "3.0 engine Cast slot=0 result=nan sameRequest=1\n"
                             "4.0 engine Cast slot=1 result=1 sameRequest=1 latencyMs=inf\n"
                             "controls inputDropped=-5 magicCalls=-2\n")
        self.assertEqual(report["unattributed"]["requested"], 1)
        self.assertEqual(report["unattributed"]["refused"], 1)
        self.assertEqual(report["slots"][1]["confirmed"], 1)
        self.assertEqual(report["latency_ms"]["count"], 0)
        self.assertEqual(report["input_dropped"], 0)
        self.assertFalse(report["native_magic_observed"])
        json.dumps(report, allow_nan=False)

    def test_native_counter_is_evidence_even_without_magic_lines(self):
        report = analyze_log("1.0 cast request=1 slot=0 REQUEST\n"
                             "2.0 player AI after npcMode=7 magicCalls=17\n")
        self.assertTrue(report["native_magic_observed"])
        self.assertFalse(report["missing_native_magic"])
        self.assertEqual(report["pending_request_ids"], [1])

    def test_jump_lifecycle_counts_each_phase_once_and_measures_only_landing(self):
        report = analyze_log("""0.0 jump id=1 seq=2 phase=requested reason=press held=1
0.0 jump id=1 seq=2 phase=requested reason=press held=1
0.1 jump id=1 seq=2 phase=refused reason=no_contact
0.2 jump id=1 seq=2 phase=accepted reason=accepted physics=1 vy=370.6
0.9 jump id=1 seq=2 phase=landed reason=landed ageSec=0.76 peakCm=69.8
0.9 jump id=1 seq=2 phase=landed reason=landed ageSec=0.76 peakCm=69.8
1.0 jump id=2 seq=3 phase=requested reason=press
1.1 jump id=2 seq=3 phase=accepted reason=accepted
1.2 jump id=2 seq=3 phase=interrupted reason=native_action ageSec=0.1 peakCm=5
2.0 jump id=3 seq=4 phase=requested reason=press
2.2 jump id=3 seq=4 phase=expired reason=no_contact
3.0 jump id=4 seq=5 phase=requested reason=press
3.1 jump id=4 seq=5 phase=accepted reason=accepted
""")
        jumps = report["jumps"]
        self.assertEqual(jumps["phases"], dict(requested=4, accepted=3, refused=1,
                                               landed=1, interrupted=1, expired=1))
        self.assertEqual(jumps["reasons"], dict(no_contact=2, native_action=1))
        self.assertEqual(jumps["landed_peak_cm"]["mean"], 69.8)
        self.assertEqual(jumps["landed_airtime_sec"]["mean"], 0.76)
        self.assertEqual(jumps["pending_ids"], [4])
        self.assertEqual(jumps["active_ids"], [4])

    def test_camera_and_motion_are_observed_samples_not_inferred_passes(self):
        report = analyze_log("""0.0 controls seq=2 cameraDist=300 cameraWant=300 pivotHeight=75 cameraBlocked=0 roofLimited=0 desiredX=540 desiredZ=0 actualX=530 actualZ=0 vy=-20 movementBlocked=0
0.5 controls seq=2 cameraDist=15 cameraWant=15 pivotHeight=-20 cameraBlocked=1 roofLimited=1 desiredX=540 desiredZ=0 actualX=0 actualZ=0 vy=0 movementBlocked=1
1.0 controls seq=2 cameraDist=30 cameraWant=30 pivotHeight=-15 cameraBlocked=1 roofLimited=1 desiredX=10 desiredZ=0 actualX=0 actualZ=0 vy=nan movementBlocked=0
1.1 camera seq=2 blocked=1 roofLimited=1
1.2 movement seq=2 blocked=1 reason=collision
""")
        camera = report["camera"]
        self.assertEqual(camera["samples"], 3)
        self.assertEqual(camera["blocked_samples"], 2)
        self.assertEqual(camera["roof_limited_samples"], 2)
        self.assertEqual(camera["blocked_event_lines"], 1)
        self.assertEqual(camera["distance_cm"]["min"], 15.0)
        self.assertEqual(camera["pivot_above_head_cm"]["min"], -20.0)
        movement = report["movement"]
        self.assertEqual(movement["samples"], 3)
        self.assertEqual(movement["blocked_samples"], 1)
        self.assertEqual(movement["low_progress_samples"], 1)
        self.assertEqual(movement["blocked_event_lines"], 1)
        self.assertEqual(movement["vertical_speed_cm_s"]["min"], -20)
        self.assertIn("nie dowodza", camera["limitation"])

    def test_missing_or_malformed_motion_measurements_are_not_zero_height_passes(self):
        report = analyze_log("""1.0 jump id=nan phase=landed peakCm=70 ageSec=0.76
2.0 jump id=1 phase=landed peakCm=nan ageSec=-1
3.0 jump id=2 phase=landed peakCm=-4 ageSec=inf
4.0 controls seq=1 cameraDist=nan pivotHeight=inf desiredX=540 desiredZ=0
5.0 controls seq=1 cameraDist=-1 actualX=1 actualZ=2
6.0 controls seq=1 desiredX=1.79e308 desiredZ=1.79e308 actualX=1 actualZ=2
""")
        self.assertEqual(report["jumps"]["phases"]["landed"], 2)
        self.assertEqual(report["jumps"]["landed_peak_cm"]["count"], 0)
        self.assertIsNone(report["jumps"]["landed_airtime_sec"]["mean"])
        self.assertEqual(report["camera"]["samples"], 0)
        self.assertEqual(report["movement"]["samples"], 0)
        json.dumps(report, allow_nan=False)

    def test_v5_aim_releases_are_unique_but_snapshots_can_share_an_input_sequence(self):
        report = analyze_log("""0.0 build PH_CAST_V5_BODY_AIM_AVADA_READY
1.0 aim seq=10 phase=release mode=assist target=34C50000 reason=acquired angle=2.5 eligible=3 tested=1
1.0 aim seq=10 phase=release mode=assist target=34C50000 reason=acquired angle=2.5 eligible=3 tested=1
1.1 aim seq=11 phase=release mode=ray target=0x34c50000 reason=acquired angle=0
1.2 aim seq=12 phase=release mode=none target=00000000 reason=outside_assist angle=13
1.3 aim seq=13 phase=release mode=none target=(nil) reason=camera_occluded angle=0
1.4 aim seq=0 phase=snapshot mode=none target=00000000 reason=no_target ready=0
1.5 aim seq=13 phase=snapshot mode=assist target=34C50000 reason=acquired ready=0
2.0 aim seq=13 phase=snapshot mode=assist target=34C50000 reason=acquired ready=1
""")
        aim = report["aim"]
        self.assertTrue(aim["observed"])
        self.assertEqual(aim["release_samples"], 4)
        self.assertEqual(aim["acquired_releases"], 2)
        self.assertEqual(aim["missed_releases"], 2)
        self.assertEqual(aim["release_modes"], dict(ray=1, assist=1))
        self.assertEqual(aim["miss_reasons"], dict(outside_assist=1, camera_occluded=1))
        self.assertEqual(aim["snapshot_samples"], 3)
        self.assertEqual(aim["acquired_snapshots"], 2)
        self.assertEqual(aim["dimmed_snapshots"], 1)
        self.assertEqual(aim["malformed_samples"], 0)
        self.assertEqual(report["totals"]["confirmed"], 0)
        self.assertIn("nie dowodzi rzutu", aim["limitation"])

    def test_null_aim_pointers_never_count_as_acquired(self):
        nulls = ("00000000", "0000000000000000", "0x0", "0X00000000", "(nil)")
        trace = ""
        for sequence, pointer in enumerate(nulls, 1):
            trace += f"aim seq={sequence} phase=release mode=none target={pointer} reason=outside_assist\n"
            trace += f"aim seq={sequence + 10} phase=release mode=assist target={pointer} reason=acquired\n"
        aim = analyze_log(trace)["aim"]
        self.assertEqual(aim["release_samples"], len(nulls))
        self.assertEqual(aim["missed_releases"], len(nulls))
        self.assertEqual(aim["acquired_releases"], 0)
        self.assertEqual(aim["malformed_samples"], len(nulls))

    def test_incomplete_or_contradictory_aim_entries_are_not_measurements(self):
        records = (
            "seq=1 mode=assist target=34C50000 reason=acquired",
            "seq=nan phase=release mode=assist target=34C50000 reason=acquired",
            "seq=-1 phase=release mode=assist target=34C50000 reason=acquired",
            "seq=0 phase=release mode=assist target=34C50000 reason=acquired",
            "seq=1 phase=release mode=unknown target=34C50000 reason=acquired",
            "seq=1 phase=release mode=assist target=bogus reason=acquired",
            "seq=1 phase=release mode=assist target=123 reason=acquired",
            "seq=1 phase=release mode=assist target=34C50000",
            "seq=1 phase=release mode=assist target=34C50000 reason=outside_assist",
            "seq=1 phase=release mode=none target=34C50000 reason=outside_assist",
            "seq=1 phase=snapshot mode=none target=00000000 reason=acquired ready=0",
        )
        report = analyze_log("\n".join("aim " + record for record in records))
        aim = report["aim"]
        self.assertFalse(aim["observed"])
        self.assertEqual(aim["release_samples"], 0)
        self.assertEqual(aim["snapshot_samples"], 0)
        self.assertEqual(aim["dimmed_snapshots"], 0)
        self.assertEqual(aim["malformed_samples"], len(records))
        self.assertTrue(any("sprzeczne wpisy namierzania" in warning for warning in report["warnings"]))
        json.dumps(report, allow_nan=False)

    def test_invalid_aim_release_does_not_reserve_its_sequence_or_fake_dimness(self):
        aim = analyze_log("""aim seq=7 phase=release mode=assist target=not_a_pointer reason=acquired
aim seq=7 phase=release mode=assist target=00000001 reason=acquired
aim seq=7 phase=release mode=ray target=00000001 reason=acquired
aim seq=7 phase=snapshot mode=assist target=00000001 reason=acquired ready=nan
aim seq=7 phase=snapshot mode=assist target=00000001 reason=acquired
""")["aim"]
        self.assertEqual(aim["acquired_releases"], 1)
        self.assertEqual(aim["release_modes"], dict(ray=0, assist=1))
        self.assertEqual(aim["snapshot_samples"], 2)
        self.assertEqual(aim["dimmed_snapshots"], 0)
        self.assertEqual(aim["malformed_samples"], 1)

    def test_aim_cli_describes_observations_and_legacy_absence(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "ParryHotter_cast.log"
            path.write_text("aim seq=2 phase=release mode=none target=00000000 reason=target_occluded\n"
                            "aim seq=3 phase=release mode=assist target=34C50000 reason=acquired\n"
                            "aim seq=3 phase=snapshot mode=assist target=34C50000 reason=acquired ready=0\n",
                            encoding="utf-8")
            output = io.StringIO()
            with redirect_stdout(output):
                self.assertEqual(main([str(path)]), 0)
            self.assertIn("Namierzanie przy puszczeniu LPM: namierzone=1, bez_celu=1", output.getvalue())
            self.assertIn("namierzone_z_niegotowym_czarem=1", output.getvalue())
            self.assertIn("target_occluded=1", output.getvalue())
            path.write_text("0.0 build PH_CAST_INPUT_NATIVE_READY_V2\n", encoding="utf-8")
            self.assertFalse(analyze_file(path)["aim"]["observed"])
            output = io.StringIO()
            with redirect_stdout(output):
                self.assertEqual(main([str(path)]), 0)
            self.assertIn("Namierzanie: brak prawidlowych pomiarow w logu", output.getvalue())

    def test_cli_json_summary_and_missing_file(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "ParryHotter_cast.log"
            path.write_bytes(b"0.000 build PH_CAST_CONTROLS_V3\r\n")
            self.assertEqual(analyze_file(path)["build_markers"], ["PH_CAST_CONTROLS_V3"])
            output = io.StringIO()
            with redirect_stdout(output):
                self.assertEqual(main([str(path), "--json"]), 0)
            self.assertEqual(json.loads(output.getvalue())["totals"]["requested"], 0)
            output = io.StringIO()
            with redirect_stdout(output):
                self.assertEqual(main([str(path)]), 0)
            self.assertIn("Flipendo", output.getvalue())
            with redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                main([str(path.with_name("missing.log"))])
            self.assertEqual(error.exception.code, 1)

    def test_script_imports_from_outside_checkout(self):
        script = Path(__file__).resolve().parents[1] / "tools" / "analyze_controls_log.py"
        with tempfile.TemporaryDirectory() as elsewhere:
            result = subprocess.run([sys.executable, str(script), "--help"], cwd=elsewhere,
                                    capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("--json", result.stdout)


if __name__ == "__main__":
    unittest.main()
