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

if __name__ == '__main__':
    unittest.main()
