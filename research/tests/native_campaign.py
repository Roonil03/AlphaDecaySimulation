"""End-to-end native scheduler and exclusive campaign launcher contract."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
ELIXIR = ROOT / 'research' / 'elixir'


def launch_args(directory, manifest, scheduler, core):
    if os.name == 'nt':
        return ['powershell', '-NoProfile', '-File', str(ELIXIR / 'campaign.ps1'),
                '-CampaignDir', str(directory), '-Manifest', str(manifest),
                '-Scheduler', str(scheduler), '-Core', str(core), '-Workers', '2']
    return ['sh', str(ELIXIR / 'campaign.sh'), str(directory), str(manifest),
            str(scheduler), str(core), '2']


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--core', type=lambda p: Path(p).resolve(), required=True)
    parser.add_argument('--scheduler', type=lambda p: Path(p).resolve(), required=True)
    args = parser.parse_args()
    source = ROOT / 'research' / 'astro' / 'campaign.json'
    with tempfile.TemporaryDirectory() as tmp:
        base = Path(tmp)
        directory = base / 'campaign'
        directory.mkdir()
        (directory / 'paused').write_text('paused')
        command = launch_args(directory, source, args.scheduler, args.core)
        process = subprocess.Popen(command, cwd=ELIXIR, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        try:
            deadline = time.monotonic() + 90
            while not (directory / 'owner.lock').exists() and process.poll() is None and time.monotonic() < deadline:
                time.sleep(.1)
            assert (directory / 'owner.lock').exists(), 'runner did not acquire lock'
            if os.name == 'nt':
                # Wait for the BEAM writer to start before abruptly killing the wrapper.
                deadline = time.monotonic() + 90
                while not (directory / 'manifest.json').exists() and process.poll() is None and time.monotonic() < deadline:
                    time.sleep(.1)
                assert (directory / 'manifest.json').exists(), 'BEAM writer did not start'
                process.kill()
                process.communicate(timeout=30)
                process = subprocess.Popen(command, cwd=ELIXIR, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                import ctypes
                kernel = ctypes.WinDLL('kernel32', use_last_error=True)
                kernel.CreateFileW.restype = ctypes.c_void_p
                kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32,
                                               ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
                kernel.CloseHandle.argtypes = [ctypes.c_void_p]
                deadline = time.monotonic() + 90
                while process.poll() is None and time.monotonic() < deadline:
                    handle = kernel.CreateFileW(str(directory / 'owner.lock'), 0x80000000, 0, None, 3, 0, None)
                    if handle == ctypes.c_void_p(-1).value:
                        break
                    kernel.CloseHandle(handle)
                    time.sleep(.1)
                else:
                    raise AssertionError('restart did not acquire lock')
                assert process.poll() is None, 'restart failed after forced stop'
            other = subprocess.run(command, cwd=ELIXIR, capture_output=True, text=True, timeout=30)
            assert other.returncode != 0, 'second owner was accepted'
            assert not list((directory / 'results').glob('*.json')), 'paused campaign ran a case'
            (directory / 'paused').unlink()
            output, _ = process.communicate(timeout=120)
            assert process.returncode == 0, output
            results = [json.loads(p.read_text()) for p in (directory / 'results').glob('*.json')]
            assert {r['id'] for r in results} == {'po212', 'po210'}
            assert all(r['response']['schema_version'] == 1 for r in results)
            replay = subprocess.run(command, cwd=ELIXIR, capture_output=True, text=True, timeout=120)
            assert replay.returncode == 0, replay.stdout + replay.stderr
            events = [json.loads(line) for line in (directory / 'events.jsonl').read_text().splitlines()]
            assert len([e for e in events if e['event'] == 'succeeded']) == 2
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()

        bad = base / 'bad.json'
        bad.write_text(json.dumps({'cases': [{'id': 'bad', 'request': {'schema_version': 1}}]}))
        rejected = base / 'rejected'
        rejected.mkdir()
        failed = subprocess.run(launch_args(rejected, bad, args.scheduler, args.core), cwd=ELIXIR,
                                capture_output=True, text=True, timeout=120)
        assert failed.returncode != 0, 'invalid native request accepted'
        assert json.loads((rejected / 'attempts' / 'bad.json').read_text())['count'] == 1
        assert (rejected / 'attempts' / 'bad.rejected').exists()
        print('native campaign, lock, pause, replay and domain rejection passed')

if __name__ == '__main__': main()



