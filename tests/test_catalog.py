import contextlib
import io
import math
import unittest
from unittest.mock import patch
from observatory.__main__ import catalog, main, validate

class CatalogTest(unittest.TestCase):
    def test_catalog_and_branch_correction(self):
        for item in catalog():
            self.assertAlmostEqual(item['partial_half_life_s'], item['total_half_life_s'] / item['ground_state_branch_fraction'])
            self.assertGreater(item['parent_binding_mev'], 0)

    def test_validation_groups_and_errors(self):
        def fake(core, payload):
            return {'log10_half_life_s': 1, 'research_log10_half_life_s': 2, 'shell_log10_half_life_s': 3}
        with patch('observatory.__main__.calculate', side_effect=fake):
            legacy = validate('native')
            holdout = validate('native', 'ren_holdout')
        self.assertEqual(legacy['metrics']['shell']['count'], 13)
        self.assertEqual([x['isotope'] for x in holdout['nuclides']], ['Dy-154', 'Os-186'])
        self.assertAlmostEqual(holdout['nuclides'][0]['shell_error'], 3 - math.log10(44180640000000.0))

    def test_cli_sweep_endpoints(self):
        for q, model in (('0.11', 'baseline'), ('99.9', 'research')):
            calls = []
            def fake(core, payload):
                calls.append(payload)
                return {'log10_half_life_s': 0}
            args = ['observatory', '--z', '84', '--a', '212', '--q', q, '--binding', '10000', '--model', model]
            with self.subTest(q=q), patch('sys.argv', args), patch('observatory.__main__.calculate', side_effect=fake), contextlib.redirect_stdout(io.StringIO()): main()
            self.assertEqual(calls[0]['q_mev'] if q == '0.11' else calls[-1]['q_mev'], .01 if q == '0.11' else 100)
            self.assertEqual('parent_binding_mev' in calls[0], model == 'research')

if __name__ == '__main__': unittest.main()
