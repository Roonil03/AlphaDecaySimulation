import json
import math
import subprocess
import sys
import unittest

EXE = sys.argv.pop(1)
BASE = dict(schema_version=1, parent_z=84, parent_a=212, q_mev=8.9542, l=0,
            radius_fm=9.0, preformation=0.1, assault_hz=1e21,
            research_model=True, parent_binding_mev=1655.7716856)

class Contract(unittest.TestCase):
    def run_request(self, request):
        return subprocess.run([EXE, '--run'], input=json.dumps(request), text=True, capture_output=True)

    def test_models_and_json_version(self):
        proc = self.run_request(BASE)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        result = json.loads(proc.stdout)
        self.assertEqual(result['schema_version'], 1)
        self.assertAlmostEqual(result['research_log10_half_life_s'], -6.630368823101762, places=9)
        self.assertAlmostEqual(result['shell_log10_half_life_s'], -6.500350233085122, places=9)
        self.assertTrue(all(math.isfinite(v) for v in result.values()))

    def test_gating_and_invalid_inputs(self):
        for change in ({'l': 1}, {'parent_a': 211}, {'research_model': False}):
            proc = self.run_request(BASE | change)
            self.assertEqual(proc.returncode, 0, proc.stderr)
            result = json.loads(proc.stdout)
            self.assertNotIn('research_log10_half_life_s', result)
            self.assertNotIn('shell_log10_half_life_s', result)
        for change in ({'preformation': 0}, {'schema_version': 2}, {'unknown': 1}, {'q_mev': -1}):
            proc = self.run_request(BASE | change)
            self.assertNotEqual(proc.returncode, 0)
            self.assertIn('error', json.loads(proc.stderr))

    def test_no_barrier(self):
        proc = self.run_request(BASE | {'radius_fm': 100.0})
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(json.loads(proc.stdout)['log10_penetrability'], 0.0)

    def test_integer_fields_reject_conversion_and_overflow(self):
        for field in ('schema_version', 'parent_z', 'parent_a', 'l'):
            for value in (BASE[field] + 0.9, float(BASE[field]), True, str(BASE[field]),
                          BASE[field] + 2**32, BASE[field] - 2**32,
                          2**63, 2**64 - 1, -(2**63), -(2**64)):
                with self.subTest(field=field, value=value):
                    proc = self.run_request(BASE | {field: value})
                    self.assertNotEqual(proc.returncode, 0)
                    self.assertIn(field, json.loads(proc.stderr)['error'])
        proc = self.run_request(BASE | {'l': -0.9})
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn('l', json.loads(proc.stderr)['error'])

    def test_supported_integer_bounds(self):
        for change in ({'parent_z': 3, 'parent_a': 5},
                       {'parent_z': 130, 'parent_a': 400, 'l': 100}):
            with self.subTest(change=change):
                proc = self.run_request(BASE | change)
                self.assertEqual(proc.returncode, 0, proc.stderr)

    def test_embedded_font_notice(self):
        proc = subprocess.run([EXE, '--credits'], text=True, capture_output=True)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn('ProggyClean.ttf', proc.stdout)
        self.assertIn('Copyright (c) 2004, 2005 Tristan Grimmer', proc.stdout)
        self.assertIn('The above copyright notice and this permission notice shall be included',
                      proc.stdout)

if __name__ == '__main__':
    unittest.main()
