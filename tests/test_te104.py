import unittest
from observatory.__main__ import catalog

class Te104CatalogTest(unittest.TestCase):
    def test_direct_measurement_and_distinct_historical_limits(self):
        te = next(row for row in catalog() if (row['symbol'], row['a']) == ('Te', 104))
        self.assertEqual((te['q_mev'], te['q_uncertainty_mev']), (5.23, .09))
        self.assertEqual((te['partial_half_life_s'], te['half_life_uncertainty_minus_s'],
                          te['half_life_uncertainty_plus_s']), (7.2e-9, 1.5e-9, 2.3e-9))
        self.assertEqual([row['half_life_upper_limit_s'] for row in te['historical_observations']], [18e-9, 4e-9])
        self.assertEqual(te['validation_group'], 'te104_2026')
        self.assertEqual(te['theoretical_predictions'][0]['q_mev'], 5.42)
