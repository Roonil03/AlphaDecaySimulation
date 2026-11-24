"""Execute an extracted research bundle with a separately supplied native core."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def invoke(command, directory, env, timeout=120):
    result = subprocess.run([str(part) for part in command], cwd=directory, env=env,
                            text=True, capture_output=True, timeout=timeout)
    if result.returncode:
        raise RuntimeError(f'{command[0]} failed:\n{result.stdout}\n{result.stderr}')
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=lambda p: Path(p).resolve(), required=True)
    parser.add_argument('--core', type=lambda p: Path(p).resolve(), required=True)
    args = parser.parse_args()
    env = os.environ.copy()
    env.pop('PYTHONPATH', None)
    with tempfile.TemporaryDirectory(prefix='alpha-bundle-') as tmp:
        isolated = Path(tmp)
        shutil.unpack_archive(args.archive, isolated)
        package = isolated / 'AlphaDecay-research'
        scheduler = package / ('scheduler.exe' if os.name == 'nt' else 'scheduler')
        assert scheduler.is_file(), 'packaged scheduler missing'
        assert not (package / 'native').exists(), 'smoke must use the external core'
        manifest = package / 'research' / 'astro' / 'campaign.json'
        result = json.loads(invoke([scheduler, '-core', args.core, '-input', manifest], isolated, env))
        assert [row['id'] for row in result] == ['po212', 'po210']
        assert all(row['response']['schema_version'] == 1 for row in result)

        observed = json.loads(invoke([sys.executable, '-m', 'observatory', '--core', args.core,
                                      '--population', '10'], package, env))
        assert len(observed['energy_sweep']) == 3
        output, plot = isolated / 'fit.json', isolated / 'fit.png'
        invoke([sys.executable, package / 'research' / 'python' / 'fit.py',
                package / 'research' / 'python' / 'data' / 'example-v1.json',
                '--core', args.core, '--output', output, '--plot', plot], isolated, env)
        fitted = json.loads(output.read_text())
        assert fitted['dataset_version'] == 'example-1' and fitted['calibration_ids'] == ['example-po212']
        assert plot.read_bytes().startswith(b'\x89PNG\r\n\x1a\n')

        elixir = package / 'research' / 'elixir'
        mix = shutil.which('mix') or shutil.which('mix.bat')
        if mix is None:
            raise RuntimeError('Elixir mix is required for the bundle smoke')
        invoke([mix, 'deps.get'], elixir, env)
        campaign = isolated / 'campaign'
        if os.name == 'nt':
            command = ['powershell', '-NoProfile', '-File', elixir / 'campaign.ps1',
                       '-CampaignDir', campaign, '-Manifest', manifest,
                       '-Scheduler', scheduler, '-Core', args.core, '-Workers', '2']
        else:
            command = ['sh', elixir / 'campaign.sh', campaign, manifest, scheduler, args.core, '2']
        invoke(command, elixir, env)
        rows = [json.loads(path.read_text()) for path in (campaign / 'results').glob('*.json')]
        assert {row['id'] for row in rows} == {'po212', 'po210'}
        assert all(row['response']['schema_version'] == 1 for row in rows)
        print('extracted scheduler, observatory, fitter/plot and campaign passed with external core')


if __name__ == '__main__':
    main()
