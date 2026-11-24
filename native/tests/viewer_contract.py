"""Preview input failures must be diagnosed without needing a graphics display."""
import json
import os
import subprocess
import sys

exe = os.path.abspath(sys.argv[1])
env = os.environ.copy()
env.pop("DISPLAY", None)
cases = [
    (["--render-preview"], "usage:"),
    (["--render-preview", os.devnull, "Missing-999"], "unknown catalog id"),
    (["--render-preview", os.devnull, "Po-212", "640junk", "480"], "invalid preview dimension"),
    (["--render-preview", os.devnull, "Po-212", "639", "480"], "preview size outside"),
    (["--render-preview", os.devnull, "Po-212", "640", "480", "unknown"], "unknown preview potential"),
    (["--render-preview", os.devnull, "Po-212", "640", "480", "external", "0", "0"], "deformed preview requires"),
    (["--render-preview", os.devnull, "Po-212", "640", "480", "woods_saxon", "0.2junk", "0"], "invalid preview deformation"),
    (["--render-preview", os.devnull, "Po-212", "640", "480", "woods_saxon", "0.6", "0"], "deformation"),
    (["--render-preview", os.devnull, "Po-212", "640", "480", "woods_saxon", "nan", "0"], "deformation"),
]
for args, expected in cases:
    run = subprocess.run([exe, *args], text=True, capture_output=True, env=env, timeout=15)
    assert run.returncode != 0, (args, run.stdout, run.stderr)
    assert not run.stdout.strip(), (args, run.stdout)
    error = json.loads(run.stderr)
    assert expected.lower() in error["error"].lower(), (args, error)
print(f"{len(cases)} preview argument rejection cases passed without DISPLAY")
