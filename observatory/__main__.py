"""Run a reproducible Po-212 benchmark against the HTTP services."""
import argparse
import json
import math
import time
import urllib.error
import urllib.request

# ENSDF 2020 evaluation and Belli et al. (2021); see README for provenance.
Q_MEV = 8.95420
MEASURED_HALF_LIFE_S = 295.1e-9
BASE = {"parent_z": 84, "parent_a": 212, "q_mev": Q_MEV, "l": 0,
        "radius_fm": 9.0, "preformation": 0.1, "assault_hz": 1e21}
TIMES = [0.0, 1e-7, 3e-7, 6e-7, 1e-6]


def request(base, path, payload=None):
    data = None if payload is None else json.dumps(payload, allow_nan=False).encode()
    req = urllib.request.Request(base.rstrip('/') + path, data=data,
                                 headers={'Content-Type': 'application/json'})
    with urllib.request.urlopen(req, timeout=5) as response:
        return json.load(response)


def ready(base, timeout=60):
    deadline = time.monotonic() + timeout
    while True:
        try:
            return request(base, '/health')
        except (OSError, ValueError, urllib.error.URLError):
            if time.monotonic() >= deadline:
                raise RuntimeError(f'{base} did not become healthy in {timeout}s')
            time.sleep(0.5)


def barrier(base, q):
    return request(base, '/v1/barrier', {**BASE, 'q_mev': q})


def run(base, population=10000, seed=42):
    ready(base)
    points = []
    for offset in (-0.1, 0.0, 0.1):
        q = Q_MEV + offset
        result = barrier(base, q)
        points.append({'q_mev': q, **result,
                       'log10_ratio_to_measured': result['log10_half_life_s'] - math.log10(MEASURED_HALF_LIFE_S)})
    central = points[1]
    survival = request(base, '/v1/ensemble', {
        'log10_half_life_s': central['log10_half_life_s'], 'times_s': TIMES,
        'population': population, 'seed': seed})
    return {'benchmark': 'Po-212 ground-state alpha decay',
            'measured_half_life_s': MEASURED_HALF_LIFE_S,
            'inputs': BASE, 'population': population, 'seed': seed,
            'energy_sweep': points, 'times_s': TIMES, 'survival': survival}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', default='http://127.0.0.1:8080', help='service base URL')
    parser.add_argument('--population', type=int, default=10000)
    parser.add_argument('--seed', type=int, default=42)
    args = parser.parse_args()
    if args.population <= 0:
        parser.error('population must be positive')
    print(json.dumps(run(args.url, args.population, args.seed), indent=2, allow_nan=False))


if __name__ == '__main__':
    main()

