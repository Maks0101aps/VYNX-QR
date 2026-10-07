#!/usr/bin/env python3
"""Collect exactly five tested binary packages and one unified checksum manifest."""
import argparse
import hashlib
import importlib.util
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location('version', ROOT / 'scripts/project-version.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    version = module.project_version(ROOT)
    expected = {
        'vynx-qr-release': [f'VYNX-QR-Setup-x64-{version}.exe', f'VYNX-QR-Portable-x64-{version}.zip'],
        'debian-deb': [f'vynx-qr_{version}_amd64.deb'],
        'arch-package': [f'vynx-qr-{version}-1-x86_64.pkg.tar.zst'],
        'linux-appimage': [f'VYNX-QR-{version}-x86_64.AppImage'],
    }
    args.output.mkdir(parents=True, exist_ok=True)
    assert not list(args.output.iterdir()), 'public destination must be empty'
    lines = []
    for artifact, names in expected.items():
        directory = args.input / artifact
        assert {p.name for p in directory.iterdir()} == set(names) | {'SHA256SUMS.txt'}, artifact
        manifest = (directory / 'SHA256SUMS.txt').read_text().splitlines()
        assert len(manifest) == len(names), artifact
        for name in names:
            file = directory / name
            digest = hashlib.sha256(file.read_bytes()).hexdigest()
            line = f'{digest}  {name}'
            assert line in manifest, f'checksum mismatch: {name}'
            shutil.copy2(file, args.output / name)
            lines.append(line)
    assert len(lines) == 5
    (args.output / 'SHA256SUMS.txt').write_text('\n'.join(sorted(lines)) + '\n')
    print('\n'.join(sorted(lines)))


if __name__ == '__main__':
    main()
