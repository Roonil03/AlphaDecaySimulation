"""Simulate a selected alpha emitter or compare the published benchmark catalog."""
import argparse
import json
import math
from pathlib import Path
import time
import urllib.error
import urllib.request

CATALOG = Path(__file__).with_name('isotopes.json')
Q_MEV = 8.95420
MEASURED_HALF_LIFE_S = 2.951e-7
BASE = {'parent_z': 84, 'parent_a': 212, 'q_mev': Q_MEV, 'l': 0,
        'radius_fm': 9.0, 'preformation': 0.1, 'assault_hz': 1e21}
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
            with urllib.request.urlopen(base.rstrip('/') + '/health', timeout=5):
                return
        except (OSError, urllib.error.URLError):
            if time.monotonic() >= deadline:
                raise RuntimeError(f'{base} did not become healthy in {timeout}s')
            time.sleep(0.5)


def request_when_ready(base, path, payload, timeout=60):
    deadline = time.monotonic() + timeout
    while True:
        try:
            return request(base, path, payload)
        except urllib.error.HTTPError as error:
            if error.code not in (502, 503, 504):
                raise
        except (OSError, urllib.error.URLError):
            pass
        if time.monotonic() >= deadline:
            raise RuntimeError(f'{base}{path} did not become ready in {timeout}s')
        time.sleep(0.5)


def catalog():
    return json.loads(CATALOG.read_text(encoding='utf-8-sig'))


def barrier(base, q):
    return request_when_ready(base, '/v1/barrier', {**BASE, 'q_mev': q})


def run(base, population=10000, seed=42, isotope=None, custom=None, model='research'):
    """Run an energy sweep and a seeded survival simulation for one channel."""
    ready(base)
    item = isotope or (None if custom else next(x for x in catalog() if x['symbol'] == 'Po' and x['a'] == 212))
    inputs = {**BASE, **({'parent_z': item['z'], 'parent_a': item['a'], 'q_mev': item['q_mev']} if item else custom or {})}
    # Omit the new flag for explicit baseline, preserving compatibility with older services.
    if model == 'research':
        inputs['research_model'] = True
        if item and 'parent_binding_mev' in item:
            inputs['parent_binding_mev'] = item['parent_binding_mev']
    measured = item['partial_half_life_s'] if item else None
    points = []
    for offset in (-0.1, 0.0, 0.1):
        q = inputs['q_mev'] + offset
        result = request_when_ready(base, '/v1/barrier', {**inputs, 'q_mev': q})
        if measured is not None:
            result['log10_ratio_to_measured'] = result['log10_half_life_s'] - math.log10(measured)
            if 'research_log10_half_life_s' in result:
                result['research_log10_ratio_to_measured'] = result['research_log10_half_life_s'] - math.log10(measured)
        points.append({'q_mev': q, **result})
    central = points[1]
    selected = next((key for key in ('shell_log10_half_life_s', 'research_log10_half_life_s', 'log10_half_life_s') if key in central), 'log10_half_life_s') if model == 'research' else 'log10_half_life_s'
    log_t = central[selected]
    # Times are restricted by the ensemble service; extreme estimates saturate the display scale.
    half_life = 10 ** max(-300, min(299, log_t))
    times = [half_life * x for x in (0, .25, 1, 2, 4)]
    survival = request_when_ready(base, '/v1/ensemble', {
        'log10_half_life_s': log_t, 'times_s': times,
        'population': population, 'seed': seed})
    return {'benchmark': f"{item['symbol']}-{item['a']} ground-state alpha decay" if item else 'custom alpha decay',
            'reference': item, 'measured_half_life_s': measured,
            'research_scope_note': 'Qi alpha-only fit used even-even nuclei with Z=78..108; results outside that range are extrapolations.' if not 78 <= inputs['parent_z'] <= 108 else None,
            'inputs': inputs, 'selected_model': selected, 'population': population, 'seed': seed,
            'energy_sweep': points, 'times_s': times, 'survival': survival}


def validate(base):
    ready(base)
    rows = []
    for item in catalog():
        params = {**BASE, 'parent_z': item['z'], 'parent_a': item['a'], 'q_mev': item['q_mev'], 'research_model': True, 'parent_binding_mev': item['parent_binding_mev']}
        result = request_when_ready(base, '/v1/barrier', params)
        ref = math.log10(item['partial_half_life_s'])
        rows.append({'isotope': f"{item['symbol']}-{item['a']}", 'reference_log10_s': ref, 'qi_extrapolation': not 78 <= item['z'] <= 108,
                     'baseline_log10_s': result['log10_half_life_s'],
                     'research_log10_s': result['research_log10_half_life_s'],
                     'baseline_error': result['log10_half_life_s'] - ref,
                     'research_error': result['research_log10_half_life_s'] - ref,
                     'shell_log10_s': result.get('shell_log10_half_life_s'),
                     'shell_error': result['shell_log10_half_life_s'] - ref if 'shell_log10_half_life_s' in result else None})
    metrics = {}
    for model in ('baseline', 'research', 'shell'):
        errors = [x[model + '_error'] for x in rows if x[model + '_error'] is not None]
        metrics[model] = {'count': len(errors), 'mae_log10': sum(map(abs, errors))/len(errors) if errors else None,
                          'rmse_log10': math.sqrt(sum(x*x for x in errors)/len(errors)) if errors else None}
    return {'description': 'Ground-state partial half-life comparison; errors are log10(prediction/reference).',
            'inputs': {'l': 0, 'radius_fm': 9.0, 'preformation': .1, 'assault_hz': 1e21},
            'metrics': metrics, 'nuclides': rows}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', default='http://127.0.0.1:8080')
    parser.add_argument('--population', type=int, default=10000)
    parser.add_argument('--seed', type=int, default=42)
    parser.add_argument('--isotope', help='catalog entry, e.g. Ra-224 (default Po-212)')
    parser.add_argument('--z', type=int, help='custom parent proton number; requires --a and --q')
    parser.add_argument('--a', type=int, help='custom parent mass number')
    parser.add_argument('--q', type=float, help='custom decay energy in MeV')
    parser.add_argument('--binding', type=float, help='custom parent total nuclear binding energy in MeV')
    parser.add_argument('--model', choices=('research', 'baseline'), default='research')
    parser.add_argument('--validate', action='store_true', help='compare every catalog isotope with both models')
    args = parser.parse_args()
    if not 1 <= args.population <= 100000:
        parser.error('population must be between 1 and 100000')
    if not -(2**63) <= args.seed < 2**63:
        parser.error('seed must be a signed 64-bit integer')
    vals = (args.z, args.a, args.q)
    if any(x is not None for x in vals) and (any(x is None for x in vals) or args.isotope):
        parser.error('provide all of --z, --a, --q together, without --isotope')
    custom = dict(parent_z=args.z, parent_a=args.a, q_mev=args.q) if args.z is not None else None
    if args.binding is not None:
        if not custom or not math.isfinite(args.binding) or args.binding <= 0:
            parser.error('--binding requires custom Z/A/Q and a positive finite energy')
        custom['parent_binding_mev'] = args.binding
    if custom and (not 3 <= args.z <= 130 or not 5 <= args.a <= 400 or args.a - args.z < 2 or not math.isfinite(args.q) or not .11 <= args.q <= 99.9):
        parser.error('custom Z, A or Q outside supported domain (Q must permit the ±0.1 MeV sweep)')
    item = None
    if args.isotope:
        item = next((x for x in catalog() if f"{x['symbol']}-{x['a']}".lower() == args.isotope.lower()), None)
        if item is None:
            parser.error('unknown isotope; see observatory/isotopes.json')
    try:
        result = validate(args.url) if args.validate else run(args.url, args.population, args.seed, item, custom, args.model)
        print(json.dumps(result, indent=2, allow_nan=False))
    except (ValueError, OverflowError) as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()




