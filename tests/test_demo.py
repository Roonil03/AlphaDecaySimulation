import unittest
from unittest.mock import patch
from observatory.__main__ import Q_MEV, run


class DemoTest(unittest.TestCase):
    def test_demo_pairs_sweep_and_survival(self):
        def fake_request(base, path, payload=None):
            if path == '/health':
                return {'status': 'ok'}
            if path == '/v1/barrier':
                return {'log10_half_life_s': -payload['q_mev'],
                        'log10_penetrability': -15, 'outer_turning_fm': 26}
            self.assertEqual(payload['log10_half_life_s'], -Q_MEV)
            return {'expected_survivors': [10000], 'sampled_survivors': [10000]}
        with patch('observatory.__main__.request', side_effect=fake_request):
            result = run('http://example', seed=42)
        self.assertEqual(len(result['energy_sweep']), 3)
        self.assertEqual(result['energy_sweep'][1]['q_mev'], Q_MEV)
        self.assertEqual(result['seed'], 42)


if __name__ == '__main__':
    unittest.main()
