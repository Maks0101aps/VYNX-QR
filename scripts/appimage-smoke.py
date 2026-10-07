#!/usr/bin/env python3
"""Audit and launch the actual downloaded AppImage on a fresh ordinary-user runner."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def run(*args, **kwargs):
    return subprocess.run(args, check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    assert os.geteuid() != 0, 'ordinary-user runtime required'
    image = args.image.resolve()
    args.report.parent.mkdir(parents=True, exist_ok=True)
    result = {'artifact': image.name, 'success': False}
    try:
        digest = hashlib.sha256(image.read_bytes()).hexdigest()
        assert f'{digest}  {image.name}' in args.manifest.read_text().splitlines(), 'artifact hash mismatch'
        result['sha256'] = digest
        result['size_bytes'] = image.stat().st_size
        image.chmod(0o755)
        version = subprocess.check_output([sys.executable, 'scripts/project-version.py'], text=True).strip()
        assert subprocess.check_output([str(image), '--version'], text=True).strip() == f'VYNX QR {version}'
        with tempfile.TemporaryDirectory(prefix='vynx AppImage audit ') as stage:
            run(str(image), '--appimage-extract', cwd=stage, stdout=subprocess.DEVNULL)
            appdir = Path(stage) / 'squashfs-root'
            docs = appdir / 'usr/share/doc/vynx-qr'
            assert (appdir / 'usr/plugins/platforms/libqxcb.so').is_file()
            assert list((appdir / 'usr/plugins/platforms').glob('libqwayland*.so'))
            run('desktop-file-validate', str(appdir / 'usr/share/applications/vynx-qr.desktop'))
            run(sys.executable, 'scripts/verify-rust-notices.py', str(docs / 'licenses/rust'),
                '--target', 'x86_64-unknown-linux-gnu', '--lockfile', 'bridge/Cargo.lock')
            for category in ('qt', 'appimage-runtime'):
                directory = docs / 'licenses' / category
                manifest = json.loads((directory / 'manifest.json').read_text())
                for name, pin in manifest.items():
                    assert hashlib.sha256((directory / name).read_bytes()).hexdigest() == pin['sha256'], name
            system = docs / 'licenses/system'
            manifest = json.loads((system / 'manifest.json').read_text())
            for dependency in manifest['sdk_dependencies']:
                for filename, sha in dependency['files'].items():
                    assert hashlib.sha256((system / filename).read_bytes()).hexdigest() == sha
            for package in manifest['packages']:
                name = package['package']
                assert hashlib.sha256((system / f'{name}-copyright').read_bytes()).hexdigest() == package['notice_sha256']
                for filename, sha in package['common_licenses'].items():
                    assert hashlib.sha256((system / 'common-licenses' / filename).read_bytes()).hexdigest() == sha
                for filename, sha in package['sources'].items():
                    assert hashlib.sha256((system / 'source' / name / filename).read_bytes()).hexdigest() == sha
            env = os.environ | {'LD_LIBRARY_PATH': str(appdir / 'usr/lib')}
            linked = subprocess.check_output(['ldd', str(appdir / 'usr/bin/vynx-qr')], env=env, text=True)
            assert 'not found' not in linked, linked
            assert all(str(appdir) in row for row in linked.splitlines() if 'libQt6' in row), linked
            args.report.with_suffix('.ldd.txt').write_text(linked)
        # No extraction fallback: FUSE launch failures remain failures.
        with tempfile.TemporaryDirectory(prefix='vynx AppImage config ') as config:
            settings = Path(config) / 'VYNX/QR/settings.json'
            settings.parent.mkdir(parents=True)
            seed = {'theme': 'dark', 'defaultSize': 512, 'clipboardCheck': False}
            settings.write_text(json.dumps(seed))
            gui_report = args.report.with_suffix('.runtime.json')
            run('xvfb-run', '-a', sys.executable, 'scripts/linux-window-smoke.py', str(image),
                '--appimage', '--config-home', config, '--report', str(gui_report))
            runtime = json.loads(gui_report.read_text())
            assert runtime['success'] and len(runtime['samples']) == 5
            saved = json.loads(settings.read_text())
            for key, value in seed.items():
                assert saved[key] == value, f'preference changed: {key}'
            result['runtime'] = runtime
        result['success'] = True
    except Exception as error:
        result['error'] = str(error)
        raise
    finally:
        args.report.write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
