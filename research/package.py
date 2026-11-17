"""Package the optional local research sources and prebuilt scheduler."""
import argparse
import os
from pathlib import Path
import shutil
import tarfile
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scheduler', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        stage = Path(tmp) / 'AlphaDecay-research'
        stage.mkdir()
        ignore = shutil.ignore_patterns('_build', 'deps', '.lake', '__pycache__', '*.pyc', '.venv')
        shutil.copytree(ROOT / 'research', stage / 'research', ignore=ignore)
        shutil.copytree(ROOT / 'observatory', stage / 'observatory', ignore=ignore)
        shutil.copy2(ROOT / 'CREDITS.md', stage / 'CREDITS.md')
        binary = stage / args.scheduler.name
        shutil.copy2(args.scheduler, binary)
        if os.name != 'nt':
            binary.chmod(binary.stat().st_mode | 0o111)
            with tarfile.open(args.output, 'w:gz') as archive:
                archive.add(stage, arcname=stage.name)
        else:
            with zipfile.ZipFile(args.output, 'w', zipfile.ZIP_DEFLATED) as archive:
                for item in stage.rglob('*'):
                    if item.is_file(): archive.write(item, item.relative_to(stage.parent))

if __name__ == '__main__': main()
