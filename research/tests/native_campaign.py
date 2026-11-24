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


def windows_active_child_lifecycle(base, scheduler):
    """Stop only our benign fixture tree and verify both children exited."""
    import ctypes
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.OpenProcess.restype = ctypes.c_void_p
    kernel.OpenProcess.argtypes = [ctypes.c_uint32, ctypes.c_int, ctypes.c_uint32]
    kernel.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
    kernel.TerminateProcess.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]
    core = base / 'waiting-core.exe'
    subprocess.run(['go', 'build', '-o', str(core), str(ROOT / 'research' / 'tests' / 'fixtures' / 'waiting_core.go')],
                   check=True, capture_output=True, text=True, timeout=120)
    directory = base / 'active-campaign'
    started, release, finished = (base / name for name in ('started.json', 'release', 'finished'))
    manifest = base / 'active.json'
    manifest.write_text(json.dumps({'cases': [{'id': 'waiting', 'request': {
        'started': str(started), 'release': str(release), 'finished': str(finished)}}]}))
    command = launch_args(directory, manifest, scheduler, core)
    process = subprocess.Popen(command, cwd=ELIXIR, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    handles = []
    try:
        deadline = time.monotonic() + 90
        while not started.exists() and process.poll() is None and time.monotonic() < deadline:
            time.sleep(.1)
        assert started.exists(), 'active native fixture did not start'
        # The file can become visible before its small write completes.
        while True:
            try:
                pids = json.loads(started.read_text())
                break
            except json.JSONDecodeError:
                if time.monotonic() >= deadline:
                    raise
                time.sleep(.01)
        for role in ('scheduler', 'core'):
            # Keep handles to these exact process instances even if IDs are reused.
            handle = kernel.OpenProcess(0x00100001, False, pids[role])
            assert handle, f'could not open active {role} fixture process'
            handles.append((role, handle))
            assert kernel.WaitForSingleObject(handle, 0) == 0x102, f'{role} was not active'
        assert not finished.exists(), 'waiting fixture unexpectedly finished'
        process.kill()
        process.communicate(timeout=30)
        for role, handle in handles:
            assert kernel.WaitForSingleObject(handle, 10000) == 0, f'{role} survived launcher termination'
        assert not finished.exists(), 'terminated fixture completed its workload'
        assert not list((directory / 'results').glob('*.json')), 'terminated case was committed'
        release.write_text('resume fixture')
        replay = subprocess.run(command, cwd=ELIXIR, capture_output=True, text=True, timeout=120)
        assert replay.returncode == 0, replay.stdout + replay.stderr
        result = json.loads((directory / 'results' / 'waiting.json').read_text())
        assert result['response']['fixture'] is True
        assert json.loads((directory / 'attempts' / 'waiting.json').read_text())['count'] == 2
        events = [json.loads(line) for line in (directory / 'events.jsonl').read_text().splitlines()]
        assert len([event for event in events if event['event'] == 'succeeded']) == 1
    finally:
        if process.poll() is None:
            process.kill()
        # Cleanup is restricted to the two processes opened from our fixture IDs.
        for _, handle in handles:
            if kernel.WaitForSingleObject(handle, 0) == 0x102:
                kernel.TerminateProcess(handle, 1)
            kernel.CloseHandle(handle)
        process.communicate(timeout=30)


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
        if os.name == 'nt':
            windows_active_child_lifecycle(base, args.scheduler)
            print('active scheduler/core termination and single-result restart passed')

if __name__ == '__main__': main()



