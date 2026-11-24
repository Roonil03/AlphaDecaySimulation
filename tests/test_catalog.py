import contextlib
import io
import math
import unittest
from unittest.mock import patch
from observatory.__main__ import catalog, main, validate

class CatalogTest(unittest.TestCase):
    def test_catalog_binding_uses_ame2020_per_nucleon_units(self):
        # Independently transcribed AME2020 mass_1.mas20.txt binding-energy/A
        # column (keV/nucleon), not the catalog's total MeV values.
        # https://www-nds.iaea.org/amdc/ame2020/mass_1.mas20.txt
        per_nucleon_kev = {
            ('Dy', 154): 8193.1305, ('Os', 186): 7982.8324,
            ('Sm', 146): 8293.8581, ('Sm', 148): 8279.6326,
            ('Po', 208): 7839.3568, ('Po', 210): 7834.3462,
            ('Po', 212): 7810.2438, ('Po', 214): 7785.1163,
            ('Po', 216): 7758.8205, ('Rn', 220): 7717.2552,
            ('Rn', 222): 7694.4991, ('Ra', 220): 7711.6879,
            ('Ra', 222): 7696.6931, ('Ra', 224): 7679.9236,
            ('Ra', 226): 7661.9636, ('Te', 104): 8157.3256}
        rows = {(row['symbol'], row['a']): row for row in catalog()}
        for key, source_value in per_nucleon_kev.items():
            with self.subTest(isotope=key):
                row = rows[key]
                self.assertAlmostEqual(row['parent_binding_mev'], source_value * row['a'] / 1000, places=9)
                self.assertFalse(row['binding_extrapolated'])

    def test_holdout_reference_year_conversion_and_group(self):
        # NUBASE2020 154Dy=3.0(15) My, 186Os=2.0(11) Py;
        # Ren et al. requires strictly less than 50% half-life uncertainty.
        rows = {(row['symbol'], row['a']): row for row in catalog()}
        for key, years, uncertainty_years in ((('Dy', 154), 1.40e6, .08e6),
                                              (('Os', 186), 2e15, 1.1e15)):
            with self.subTest(isotope=key):
                row = rows[key]
                self.assertEqual(row['validation_group'], 'ren_holdout')
                self.assertEqual(row['total_half_life_s'], years * 365.25 * 86400)
                self.assertEqual(row['half_life_uncertainty_s'], uncertainty_years * 365.25 * 86400)

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
