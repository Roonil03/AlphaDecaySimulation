import contextlib
import io
import math
import unittest
from unittest.mock import patch
from observatory.__main__ import catalog, main, run, validate


class CatalogTest(unittest.TestCase):
    def test_catalog_and_branch_correction(self):
        items = catalog()
        self.assertGreaterEqual(len({x['symbol'] for x in items}), 4)
        for item in items:
            self.assertAlmostEqual(item['partial_half_life_s'], item['total_half_life_s'] / item['ground_state_branch_fraction'])
            self.assertGreater(item['parent_binding_mev'], 0)
        self.assertLess(next(x for x in items if x['symbol'] == 'Ra' and x['a'] == 224)['ground_state_branch_fraction'], .96)

    def test_selected_model_drives_survival(self):
        calls = []
        def fake(base, path, payload):
            calls.append((path, payload))
            if path.endswith('barrier'):
                return {'log10_half_life_s': 2, 'research_log10_half_life_s': 1, 'shell_log10_half_life_s': 0}
            return {'expected_survivors': [], 'sampled_survivors': []}
        with patch('observatory.__main__.ready'), patch('observatory.__main__.request_when_ready', side_effect=fake):
            result = run('http://test')
        self.assertEqual(result['selected_model'], 'shell_log10_half_life_s')
        self.assertEqual(calls[-1][1]['log10_half_life_s'], 0)
        self.assertEqual(calls[-1][1]['times_s'], [0, .25, 1, 2, 4])
        self.assertIn('parent_binding_mev', calls[0][1])

    def test_validation_counts_and_errors(self):
        def fake(base, path, payload):
            return {'log10_half_life_s': 1, 'research_log10_half_life_s': 2, 'shell_log10_half_life_s': 3}
        with patch('observatory.__main__.ready'), patch('observatory.__main__.request_when_ready', side_effect=fake):
            result = validate('http://test')
        self.assertEqual(result['metrics']['shell']['count'], len(catalog()))
        first = result['nuclides'][0]
        self.assertAlmostEqual(first['shell_error'], 3 - math.log10(catalog()[0]['partial_half_life_s']))
        self.assertTrue(first['qi_extrapolation'])

    def test_cli_sweep_endpoints_and_baseline_wire_compatibility(self):
        for q, model in (('0.11', 'baseline'), ('99.9', 'research')):
            calls = []
            def fake(base, path, payload):
                if path.endswith('barrier'):
                    calls.append(payload)
                    self.assertGreaterEqual(payload['q_mev'], .01)
                    self.assertLessEqual(payload['q_mev'], 100)
                    return {'log10_half_life_s': 0}
                return {'expected_survivors': [], 'sampled_survivors': []}
            args = ['observatory', '--z', '84', '--a', '212', '--q', q,
                    '--binding', '10000', '--model', model]
            with self.subTest(q=q, model=model), patch('sys.argv', args), contextlib.redirect_stdout(io.StringIO()):
                with patch('observatory.__main__.ready'), patch('observatory.__main__.request_when_ready', side_effect=fake):
                    main()
            self.assertEqual(len(calls), 3)
            self.assertEqual(calls[0]['q_mev'] if q == '0.11' else calls[-1]['q_mev'], .01 if q == '0.11' else 100)
            for payload in calls:
                if model == 'baseline':
                    self.assertNotIn('research_model', payload)
                    self.assertNotIn('parent_binding_mev', payload)
                else:
                    self.assertEqual(payload['parent_binding_mev'], 10000)

    def test_cli_rejects_binding_outside_kernel_domain(self):
        for binding in ('0', '-1', '10001', 'nan', 'inf'):
            args = ['observatory', '--z', '84', '--a', '212', '--q', '8', '--binding', binding]
            with self.subTest(binding=binding), patch('sys.argv', args), contextlib.redirect_stderr(io.StringIO()):
                with patch('observatory.__main__.run') as mocked, self.assertRaises(SystemExit) as error:
                    main()
            self.assertEqual(error.exception.code, 2)
            mocked.assert_not_called()


if __name__ == '__main__':
    unittest.main()
