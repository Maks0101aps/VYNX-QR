#!/usr/bin/env python3
"""Verify a downloaded Arch artifact; root installs, builder runs the GUI."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import pwd
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
    assert os.geteuid() == 0, 'only installation/removal requires root'
    report = {'artifact': args.package.name, 'success': False}
    report['pacmanNoExtract'] = [line for line in Path('/etc/pacman.conf').read_text().splitlines()
                                if line.strip().startswith('NoExtract')]
    print('Container NoExtract configuration:', report['pacmanNoExtract'], flush=True)
    installed = False
    try:
        digest = hashlib.sha256(args.package.read_bytes()).hexdigest()
        assert f'{digest}  {args.package.name}' in args.manifest.read_text().splitlines()
        report['sha256'] = digest
        assert not Path('/usr/bin/vynx-qr').exists(), 'application already installed'
        inventory = subprocess.check_output(['bsdtar', '-tf', str(args.package)], text=True)
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.with_suffix('.inventory.txt').write_text(inventory)
        for name in inventory.splitlines():
            name = name.removeprefix('./')
            if name.endswith('/') or name in ('.PKGINFO', '.BUILDINFO', '.MTREE'):
                continue
            assert name == 'usr/bin/vynx-qr' or name == 'usr/share/applications/vynx-qr.desktop' or name.startswith((
                'usr/share/doc/vynx-qr/', 'usr/share/icons/hicolor/', 'usr/share/licenses/vynx-qr/')), name
            assert not name.endswith(('.o', '.a', '.cpp', '.rs'))
        info = subprocess.check_output(['bsdtar', '-xOf', str(args.package), '.PKGINFO'], text=True)
        dependencies = [line.split(' = ', 1)[1] for line in info.splitlines() if line.startswith('depend = ')]
        assert set(dependencies) == {'qt6-base', 'qt6-wayland', 'gcc-libs'}, dependencies
        # pacman -U verifies dependencies but does not fetch missing repo packages.
        # Install only the runtime dependencies declared by the actual artifact.
        run('pacman', '-S', '--needed', '--noconfirm', *dependencies)
        run('pacman', '-U', '--noconfirm', str(args.package.resolve()))
        installed = True
        version = subprocess.check_output([sys.executable, 'scripts/project-version.py'], text=True).strip()
        assert subprocess.check_output(['vynx-qr', '--version'], text=True).strip() == f'VYNX QR {version}'
        run('desktop-file-validate', '/usr/share/applications/vynx-qr.desktop')
        for size in (48, 64, 128, 256, 512):
            assert Path(f'/usr/share/icons/hicolor/{size}x{size}/apps/vynx-qr.png').is_file()
        run(sys.executable, 'scripts/verify-rust-notices.py', '/usr/share/licenses/vynx-qr/licenses/rust',
            '--target', 'x86_64-unknown-linux-gnu', '--lockfile', 'bridge/Cargo.lock')
        linked = subprocess.check_output(['ldd', '/usr/bin/vynx-qr'], text=True)
        assert 'not found' not in linked, linked
        args.report.with_suffix('.ldd.txt').write_text(linked)
        builder = pwd.getpwnam('builder')
        with tempfile.TemporaryDirectory(prefix='vynx arch smoke ') as temporary:
            directory = Path(temporary)
            os.chown(directory, builder.pw_uid, builder.pw_gid)
            preferences = directory / 'config/VYNX/QR/settings.json'
            preferences.parent.mkdir(parents=True)
            content = '{"theme":"dark","defaultSize":512,"clipboardCheck":false}\n'
            preferences.write_text(content)
            for entry in [directory / 'config', directory / 'config/VYNX', preferences.parent, preferences]:
                os.chown(entry, builder.pw_uid, builder.pw_gid)
            runtime = directory / 'runtime.json'
            run('runuser', '-u', 'builder', '--', 'xvfb-run', '-a', 'python',
                'scripts/linux-window-smoke.py', '/usr/bin/vynx-qr',
                '--config-home', str(directory / 'config'), '--report', str(runtime))
            data = json.loads(runtime.read_text())
            assert data['version'] == f'VYNX QR {version}'
            report['runtime'] = data
            assert preferences.read_text() == content
            run('pacman', '-R', '--noconfirm', 'vynx-qr')
            installed = False
            assert preferences.read_text() == content, 'preferences deleted during uninstall'
        for path in ('/usr/bin/vynx-qr', '/usr/share/applications/vynx-qr.desktop', '/usr/share/doc/vynx-qr', '/usr/share/licenses/vynx-qr'):
            assert not Path(path).exists(), path
        for size in (48, 64, 128, 256, 512):
            assert not Path(f'/usr/share/icons/hicolor/{size}x{size}/apps/vynx-qr.png').exists()
        report['success'] = True
    except Exception as error:
        report['error'] = str(error)
        raise
    finally:
        if installed:
            subprocess.run(['pacman', '-R', '--noconfirm', 'vynx-qr'])
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
