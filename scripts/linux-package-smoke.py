#!/usr/bin/env python3
"""Install a downloaded DEB as root, run it as a regular user, then remove it."""
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
    parser.add_argument('package', type=Path)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    assert os.geteuid() != 0, 'application smoke must run as an ordinary user'
    package = args.package.resolve()
    result = {'artifact': package.name, 'success': False, 'checks': []}
    installed = False
    try:
        digest = hashlib.sha256(package.read_bytes()).hexdigest()
        assert f'{digest}  {package.name}' in args.manifest.read_text().splitlines(), 'artifact hash mismatch'
        result['sha256'] = digest
        assert not Path('/usr/bin/vynx-qr').exists(), 'application already installed'
        run('dpkg-deb', '--info', str(package))
        inventory = subprocess.check_output(['dpkg-deb', '--contents', str(package)], text=True)
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.with_suffix('.inventory.txt').write_text(inventory)
        with tempfile.TemporaryDirectory(prefix='vynx package audit ') as stage:
            run('dpkg-deb', '--extract', str(package), stage)
            for file in Path(stage).rglob('*'):
                if not file.is_file():
                    continue
                relative = file.relative_to(stage).as_posix()
                assert relative == 'usr/bin/vynx-qr' or relative.startswith((
                    'usr/share/applications/', 'usr/share/icons/hicolor/', 'usr/share/doc/vynx-qr/')), relative
                assert not file.stat().st_mode & 0o002, f'world-writable file: {relative}'
                assert file.suffix not in ('.o', '.a', '.cpp', '.rs'), f'build/source file leaked: {relative}'
        run('sudo', 'apt-get', 'install', '-y', str(package))
        installed = True
        assert Path('/usr/bin/vynx-qr').is_file()
        assert os.access('/usr/bin/vynx-qr', os.X_OK)
        desktop = Path('/usr/share/applications/vynx-qr.desktop')
        run('desktop-file-validate', str(desktop))
        for size in (48, 64, 128, 256, 512):
            assert Path(f'/usr/share/icons/hicolor/{size}x{size}/apps/vynx-qr.png').is_file()
        docs = Path('/usr/share/doc/vynx-qr')
        assert (docs / 'LICENSE').is_file()
        run(sys.executable, 'scripts/verify-rust-notices.py', str(docs / 'licenses/rust'),
            '--target', 'x86_64-unknown-linux-gnu', '--lockfile', 'bridge/Cargo.lock')
        linked = subprocess.check_output(['ldd', '/usr/bin/vynx-qr'], text=True)
        assert 'not found' not in linked, linked
        args.report.with_suffix('.ldd.txt').write_text(linked)
        with tempfile.TemporaryDirectory(prefix='vynx user config ') as config:
            preferences = Path(config) / 'VYNX/QR/settings.json'
            preferences.parent.mkdir(parents=True)
            content = '{"theme":"dark","defaultSize":512,"clipboardCheck":false}\n'
            preferences.write_text(content)
            env = os.environ | {'XDG_CONFIG_HOME': config}
            # Three independent display/WM startups stress the initial EWMH
            # readiness transition; every run must succeed, with no workflow retries.
            for display in range(3):
                runtime_report = args.report.with_suffix(f'.runtime-{display}.json')
                run('xvfb-run', '-a', sys.executable, 'scripts/linux-window-smoke.py', '/usr/bin/vynx-qr',
                    '--config-home', config, '--report', str(runtime_report), env=env)
            assert preferences.read_text() == content, 'launch overwrote preferences'
            run('sudo', 'apt-get', 'remove', '-y', 'vynx-qr')
            installed = False
            assert preferences.read_text() == content, 'uninstall removed preferences'
        for path in ('/usr/bin/vynx-qr', '/usr/share/applications/vynx-qr.desktop', '/usr/share/doc/vynx-qr'):
            assert not Path(path).exists(), f'package path survived removal: {path}'
        for size in (48, 64, 128, 256, 512):
            assert not Path(f'/usr/share/icons/hicolor/{size}x{size}/apps/vynx-qr.png').exists()
        result['checks'] = ['sha256', 'inventory', 'permissions', 'install', 'desktop', 'icons',
                            'linux-rust-notices', 'dynamic-linking', 'ordinary-user-gui',
                            'close', 'settings-preserved', 'remove']
        result['success'] = True
    except Exception as error:
        result['error'] = str(error)
        raise
    finally:
        if installed:
            subprocess.run(['sudo', 'apt-get', 'remove', '-y', 'vynx-qr'])
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()
