"""Portable contract and independent survival checks against running services."""
import math
import os
import unittest

from observatory.__main__ import BASE, Q_MEV, TIMES, barrier, ready, request

KERNEL = os.environ.get('KERNEL_URL', 'http://127.0.0.1:8081')
ENSEMBLE = os.environ.get('ENSEMBLE_URL', 'http://127.0.0.1:8082')
GATEWAY = os.environ.get('GATEWAY_URL', 'http://127.0.0.1:8080')


class ServiceIntegration(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        for url in (KERNEL, ENSEMBLE, GATEWAY):
            ready(url)

    def test_barrier_and_gateway(self):
        direct = barrier(KERNEL, Q_MEV)
        through_gateway = barrier(GATEWAY, Q_MEV)
        for key in ('log10_half_life_s', 'log10_penetrability', 'outer_turning_fm'):
            self.assertTrue(math.isfinite(direct[key]))
            self.assertAlmostEqual(direct[key], through_gateway[key], places=8)
        # Coulomb turning point for l=0: Z_alpha Z_d e^2 / Q.
        self.assertAlmostEqual(direct['outer_turning_fm'], 2 * 82 * 1.43996448 / Q_MEV, delta=0.02)
        # Closed-form l=0 Coulomb WKB action, independent of numerical integration.
        b = 2 * 82 * 1.43996448 / Q_MEV
        x = BASE['radius_fm'] / b
        mu = 4 * (212 - 4) / 212 * 931.49410242
        action = math.sqrt(2 * mu * Q_MEV) * b / 197.3269804 * (math.acos(math.sqrt(x)) - math.sqrt(x * (1 - x)))
        log_p = -2 * action / math.log(10)
        self.assertAlmostEqual(direct['log10_penetrability'], log_p, delta=0.002)
        expected_log_t = math.log10(math.log(2)) - math.log10(BASE['preformation']) - math.log10(BASE['assault_hz']) - log_p
        self.assertAlmostEqual(direct['log10_half_life_s'], expected_log_t, delta=0.002)
        self.assertLess(direct['log10_penetrability'], 0)
        self.assertGreater(direct['outer_turning_fm'], BASE['radius_fm'])
        self.assertGreater(barrier(KERNEL, Q_MEV - 0.1)['log10_half_life_s'], direct['log10_half_life_s'])
        self.assertLess(barrier(KERNEL, Q_MEV + 0.1)['log10_half_life_s'], direct['log10_half_life_s'])

    def test_population(self):
        payload = {'log10_half_life_s': math.log10(3e-7), 'times_s': TIMES,
                   'population': 10000, 'seed': 42}
        direct = request(ENSEMBLE, '/v1/ensemble', payload)
        repeat = request(ENSEMBLE, '/v1/ensemble', payload)
        routed = request(GATEWAY, '/v1/ensemble', payload)
        self.assertEqual(direct, repeat)
        self.assertEqual(direct, routed)
        self.assertEqual(len(direct['sampled_survivors']), len(TIMES))
        self.assertEqual(len(direct['expected_survivors']), len(TIMES))
        for t, expected, sample in zip(TIMES, direct['expected_survivors'], direct['sampled_survivors']):
            self.assertAlmostEqual(expected, 10000 * math.exp(-math.log(2) * t / 3e-7), delta=0.01)
            self.assertIsInstance(sample, int)
            self.assertGreaterEqual(sample, 0)
            self.assertLessEqual(sample, 10000)
        self.assertEqual(direct['sampled_survivors'][0], 10000)
        self.assertTrue(all(a >= b for a, b in zip(direct['sampled_survivors'], direct['sampled_survivors'][1:])))


if __name__ == '__main__':
    unittest.main()

