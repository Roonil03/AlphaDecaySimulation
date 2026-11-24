import contextlib
import io
import json
import os
import subprocess
import sys
import unittest
from unittest.mock import patch
from observatory.__main__ import Q_MEV, calculate, main, run, survival

class DemoTest(unittest.TestCase):
    def test_native_result_requires_integer_version_one_object(self):
        for response in ({}, {'schema_version': True}, {'schema_version': 1.0},
                         {'schema_version': 2}, [], None):
            with self.subTest(response=response), patch('observatory.__main__.subprocess.run',
                    return_value=subprocess.CompletedProcess([], 0, stdout=json.dumps(response))):
                with self.assertRaisesRegex(ValueError, 'unsupported native result version'):
                    calculate('native', {})

    def test_sweep_uses_selected_model_and_seeded_survival(self):
        def fake(core, payload):
            return {'log10_half_life_s': -payload['q_mev'],
                    'research_log10_half_life_s': -payload['q_mev'],
                    'shell_log10_half_life_s': -payload['q_mev']}
        with patch('observatory.__main__.calculate', side_effect=fake):
            result = run('native', population=100, seed=42)
            again = run('native', population=100, seed=42)
        self.assertEqual(result['energy_sweep'][1]['q_mev'], Q_MEV)
        self.assertEqual(result['selected_model'], 'shell_log10_half_life_s')
        self.assertEqual(result['survival'], again['survival'])
        counts = result['survival']['sampled_survivors']
        self.assertEqual(counts[0], 100)
        self.assertEqual(counts, sorted(counts, reverse=True))

    def test_native_subprocess_contract(self):
        core = os.environ.get('ALPHADECAY_CORE')
        if not core: self.skipTest('set ALPHADECAY_CORE to test native integration')
        result = calculate(core, {'parent_z':84,'parent_a':212,'q_mev':8.9542,
                                  'l':0,'radius_fm':9,'preformation':0.1,'assault_hz':1e21})
        self.assertEqual(result['schema_version'], 1)
        self.assertLess(result['log10_penetrability'], 0)
        proc = subprocess.run([sys.executable, '-m', 'observatory', '--core', core,
                               '--population', '100'], capture_output=True, text=True, check=True)
        self.assertEqual(len(json.loads(proc.stdout)['energy_sweep']), 3)

    def test_extreme_survival_scale_is_explicit(self):
        for predicted, sampled in ((400, 299), (-400, -300), (1, 1)):
            with self.subTest(predicted=predicted), patch('observatory.__main__.calculate',
                    return_value={'log10_half_life_s': predicted}):
                result = run('native', population=10, model='baseline')
            scale = result['survival_time_scale']
            self.assertEqual(scale['predicted_log10_half_life_s'], predicted)
            self.assertEqual(scale['sampled_log10_half_life_s'], sampled)
            self.assertEqual(scale['saturated'], predicted != sampled)
            self.assertEqual(result['times_s'][2], 10 ** sampled)
            self.assertEqual(bool(scale['note']), predicted != sampled)
            self.assertIn('central catalog Q', result['energy_sweep_note'])

    def test_cli_validation(self):
        for flag, value in (('--population', '0'), ('--population', '100001'),
                            ('--seed', str(-(2**63)-1)), ('--seed', str(2**63))):
            with self.subTest(flag=flag, value=value), patch('sys.argv', ['observatory', flag, value]), contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as caught: main()
                self.assertEqual(caught.exception.code, 2)

if __name__ == '__main__': unittest.main()
