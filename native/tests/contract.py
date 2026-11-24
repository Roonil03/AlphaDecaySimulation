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
        self.assertTrue(all(math.isfinite(v) for v in result.values() if isinstance(v, (int, float))))
        self.assertEqual(result['model_parameters']['q_mev'], BASE['q_mev'])

    def test_exactly_one_request_and_clean_error_streams(self):
        for suffix in (' {}', '\nnull', ' trailing text'):
            proc = subprocess.run([EXE, '--run'], input=json.dumps(BASE) + suffix,
                                  text=True, capture_output=True)
            self.assertNotEqual(proc.returncode, 0)
            self.assertEqual(proc.stdout, '')
            self.assertIn('error', json.loads(proc.stderr))
        proc = subprocess.run([EXE, '--run'], input=json.dumps(BASE) + ' \n\t',
                              text=True, capture_output=True)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(proc.stderr, '')
        self.assertEqual(json.loads(proc.stdout)['schema_version'], 1)

    def test_malformed_json_and_nested_types(self):
        for raw in ('', 'null', '[]', '{"schema_version":',
                    json.dumps(BASE).replace('8.9542', 'NaN'),
                    json.dumps(BASE).replace('8.9542', '1e999')):
            proc = subprocess.run([EXE, '--run'], input=raw, text=True, capture_output=True)
            self.assertNotEqual(proc.returncode, 0, raw)
            self.assertEqual(proc.stdout, '')
            self.assertIn('error', json.loads(proc.stderr))
        spherical = BASE | {'potential_model': 'woods_saxon'}
        for change in ({'deformation': []}, {'channels': {}}, {'parent_spin': []},
                       {'uncertainty': []}, {'uncertainty': {'covariance': [1, 2, 3, 4]}},
                       {'uncertainty': {'standard_deviations': [None, 0, 0, 0]}}):
            proc = self.run_request(spherical | change)
            self.assertNotEqual(proc.returncode, 0, change)
            self.assertEqual(proc.stdout, '')
            self.assertIn('error', json.loads(proc.stderr))

    def test_external_real_boundaries_and_exact_coulomb_action(self):
        for field, low, high in (('q_mev', .01, 100.), ('radius_fm', .1, 100.),
                                  ('preformation', 0., 1.), ('assault_hz', 1e-300, 1e30),
                                  ('parent_binding_mev', 0., 10000.)):
            for value in (math.nextafter(low, -math.inf), math.nextafter(high, math.inf)):
                proc = self.run_request(BASE | {field: value})
                self.assertNotEqual(proc.returncode, 0, (field, value))
                self.assertEqual(proc.stdout, '')
            for value in (high,) if low == 0 else (low, high):
                proc = self.run_request(BASE | {field: value})
                self.assertEqual(proc.returncode, 0, (field, value, proc.stderr))
        # Independent closed-form l=0 action; fixture snapshots alone would not
        # detect a shared error in the quadrature and generated references.
        for q, radius in ((.01, .1), (5., 9.), (25., 9.), (100., 100.)):
            request = BASE | {'q_mev': q, 'radius_fm': radius}
            proc = self.run_request(request)
            self.assertEqual(proc.returncode, 0, proc.stderr)
            result = json.loads(proc.stdout)
            mu = 931.49410242 * 4 * (request['parent_a'] - 4) / request['parent_a']
            outer = 2 * (request['parent_z'] - 2) * 1.43996448 / q
            x = radius / outer
            action = (math.sqrt(2 * mu * q) * outer / 197.3269804 *
                      (math.acos(math.sqrt(x)) - math.sqrt(x * (1 - x)))) if x < 1 else 0.
            expected_logp = -2 * action / math.log(10)
            self.assertAlmostEqual(result['outer_turning_fm'], outer, delta=1e-9 * outer)
            self.assertAlmostEqual(result['log10_penetrability'], expected_logp, delta=1e-5)
            self.assertAlmostEqual(result['log10_half_life_s'],
                math.log10(math.log(2)) - math.log10(request['preformation']) -
                math.log10(request['assault_hz']) - expected_logp, delta=1e-5)

    def test_closed_channels_still_validate_uncertainty_options(self):
        closed = BASE | {'potential_model': 'woods_saxon',
                         'channels': [{'l': 0, 'excitation_mev': 20}]}
        for options in (True, {'bogus': 1}, {'sample_count': 0}, {'sample_count': 1.5},
                        {'standard_deviations': [-1, 0, 0, 0]},
                        {'standard_deviations': [1e308, 0, 0, 0]},
                        {'covariance': [[-1, 0, 0, 0], [0, 0, 0, 0],
                                        [0, 0, 0, 0], [0, 0, 0, 0]]}):
            proc = self.run_request(closed | {'uncertainty': options})
            self.assertNotEqual(proc.returncode, 0, options)
            self.assertEqual(proc.stdout, '')
            self.assertIn('error', json.loads(proc.stderr))

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

    def test_spherical_models_and_zero_preformation(self):
        for model in ('square_well', 'woods_saxon'):
            proc = self.run_request(BASE | {'potential_model': model, 'nuclear_depth_mev': 100,
                'nuclear_radius_fm': 9, 'charge_radius_fm': 9, 'diffuseness_fm': .65})
            self.assertEqual(proc.returncode, 0, proc.stderr)
            result = json.loads(proc.stdout)
            self.assertEqual(result['status'], 'ok')
            self.assertEqual(result['model_parameters']['nuclear_depth_mev'], 100)
            self.assertEqual(result['model_parameters']['diffuseness_fm'], .65)
            self.assertGreater(result['outer_turning_fm'], result['inner_turning_fm'])
            self.assertLess(result['log10_penetrability'], 0)
            self.assertIn('external_reference', result)
            proc = self.run_request(BASE | {'potential_model': model, 'preformation': 0})
            self.assertEqual(proc.returncode, 0, proc.stderr)
            result = json.loads(proc.stdout)
            self.assertEqual(result['status'], 'zero_preformation')
            self.assertIsNone(result['log10_half_life_s'])
            self.assertNotIn('external_reference', result)
        proc = self.run_request(BASE | {'potential_model': 'square_well', 'q_mev': 0})
        self.assertEqual(json.loads(proc.stdout)['status'], 'closed_barrier')
        for model in ('square_well', 'woods_saxon'):
            for q in (0, .001):
                with self.subTest(model=model, q=q):
                    proc = self.run_request(BASE | {'potential_model': model, 'q_mev': q})
                    self.assertEqual(proc.returncode, 0, proc.stderr)
                    result = json.loads(proc.stdout)
                    self.assertEqual(result['status'], 'closed_barrier' if q == 0 else 'ok')
                    self.assertNotIn('external_reference', result)
        proc = self.run_request(BASE | {'potential_model': 'woods_saxon', 'q_mev': 5,
                                       'nuclear_depth_mev': 1e18})
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn('10000 MeV', json.loads(proc.stderr)['error'])
        for change in ({'potential_model':'unknown'}, {'potential_model':'woods_saxon','diffuseness_fm':0},
                       {'potential_model':'square_well','centrifugal_convention':'bogus'}, {'nuclear_depth_mev':100}):
            proc = self.run_request(BASE | change)
            self.assertNotEqual(proc.returncode, 0)
            self.assertIn('error', json.loads(proc.stderr))

    def test_channels_and_deformation_contract(self):
        potential = BASE | {'potential_model': 'woods_saxon'}
        sphere = self.run_request(potential)
        self.assertEqual(sphere.returncode, 0, sphere.stderr)
        spin = {'twice_j': 0, 'parity': 1}
        request = potential | {'deformation': {'beta2': 0, 'beta4': 0},
            'parent_spin': spin, 'channels': [
                {'name': 'ground', 'daughter_spin': spin},
                {'name': 'excited', 'excitation_mev': 1, 'daughter_spin': {'twice_j': 4, 'parity': 1}},
                {'name': 'closed', 'excitation_mev': 20, 'l': 0}], 'competing_rate_hz': 1.0}
        proc = self.run_request(request)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        answer = json.loads(proc.stdout)
        self.assertIsNotNone(answer['log10_total_alpha_rate_hz'])
        self.assertIsNotNone(answer['log10_total_half_life_s'])
        rows = answer['channels']
        self.assertEqual([r['status'] for r in rows], ['ok', 'ok', 'closed'])
        self.assertEqual(rows[1]['l'], 2)
        self.assertAlmostEqual(rows[0]['log10_penetrability'], json.loads(sphere.stdout)['log10_penetrability'], places=10)
        self.assertAlmostEqual(sum(r['conditional_alpha_branch'] for r in rows), 1, places=12)
        self.assertLessEqual(sum(r['total_branch'] for r in rows), 1)
        deformed = self.run_request(potential | {'deformation': {'beta2': 0.2, 'beta4': 0.05}})
        self.assertEqual(deformed.returncode, 0, deformed.stderr)
        dr = json.loads(deformed.stdout)['channels'][0]
        self.assertEqual(dr['status'], 'ok')
        self.assertLess(dr['angular_error_log10'], 1e-4)
        self.assertLess(dr['charge_error_log10'], 1e-4)
        request['channels'][1]['l'] = 1
        result = json.loads(self.run_request(request).stdout)['channels']
        self.assertEqual(result[1]['status'], 'forbidden')
        request['channels'][1].pop('daughter_spin')
        request['channels'][1].pop('l')
        unresolved = json.loads(self.run_request(request).stdout)
        self.assertIsNone(unresolved['log10_total_half_life_s'])
        result = unresolved['channels']
        self.assertEqual(result[1]['status'], 'unknown_spin_requires_l')
        self.assertTrue(all(r['conditional_alpha_branch'] is None for r in result))
        for bad in (1.5, True, 9223372036854775808):
            for change in ({'parent_spin': {'twice_j': bad, 'parity': 1}},
                           {'channels': [{'l': bad}]}):
                proc = self.run_request(potential | change)
                self.assertNotEqual(proc.returncode, 0)
                self.assertIn('error', json.loads(proc.stderr))
        self.assertNotEqual(self.run_request(BASE | {'deformation': {'beta2': 0.1}}).returncode, 0)

    def test_deformed_square_well_is_independent_of_diffuseness(self):
        request = BASE | {'potential_model': 'square_well', 'q_mev': 5,
                          'deformation': {'beta2': .2, 'beta4': 0}}
        logs = []
        for diffuseness in (.65, .01):
            proc = self.run_request(request | {'diffuseness_fm': diffuseness})
            self.assertEqual(proc.returncode, 0, proc.stderr)
            answer = json.loads(proc.stdout)
            row = answer['channels'][0]
            self.assertEqual(row['status'], 'ok', row)
            self.assertIsNotNone(answer['log10_total_alpha_rate_hz'])
            logs.append(row['log10_penetrability'])
        self.assertAlmostEqual(*logs, places=7)

    def test_zero_preformation_rate_survives_failed_barrier_diagnostic(self):
        request = BASE | {'potential_model': 'woods_saxon', 'q_mev': 100,
                          'channels': [{'l': 0, 'preformation': 0},
                                       {'l': 0, 'excitation_mev': 95}]}
        proc = self.run_request(request)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        answer = json.loads(proc.stdout)
        zero, active = answer['channels']
        self.assertEqual(zero['barrier_status'], 'above_barrier')
        self.assertEqual(zero['status'], 'zero_preformation')
        self.assertIsNone(zero['log10_rate_hz'])
        self.assertIsNone(zero['log10_partial_half_life_s'])
        self.assertEqual(zero['conditional_alpha_branch'], 0)
        self.assertEqual(active['status'], 'ok')
        self.assertEqual(active['conditional_alpha_branch'], 1)
        self.assertAlmostEqual(answer['log10_total_alpha_rate_hz'], active['log10_rate_hz'])
        self.assertAlmostEqual(answer['log10_total_rate_hz'], active['log10_rate_hz'])
        self.assertAlmostEqual(answer['log10_total_half_life_s'], active['log10_partial_half_life_s'])
        # Zero formation cannot turn malformed or unresolved inputs into known rates.
        request['channels'][0]['assault_hz'] = 0
        proc = self.run_request(request)
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn('error', json.loads(proc.stderr))
        request['channels'][0].pop('assault_hz')
        request['channels'][0].pop('l')
        unresolved = json.loads(self.run_request(request).stdout)
        self.assertEqual(unresolved['channels'][0]['status'], 'unknown_spin_requires_l')
        self.assertIsNone(unresolved['log10_total_alpha_rate_hz'])

    def test_embedded_font_notice(self):
        proc = subprocess.run([EXE, '--credits'], text=True, capture_output=True)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn('ProggyClean.ttf', proc.stdout)
        self.assertIn('Copyright (c) 2004, 2005 Tristan Grimmer', proc.stdout)
        self.assertIn('The above copyright notice and this permission notice shall be included',
                      proc.stdout)

if __name__ == '__main__':
    unittest.main()
