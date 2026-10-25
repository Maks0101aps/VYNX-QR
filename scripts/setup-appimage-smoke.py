#!/usr/bin/env python3
"""Drive the actual Linux Setup AppImage, launch its installation and uninstall it."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time


def run(*args, **kwargs):
    return subprocess.run(args, check=True, **kwargs)


def window(title, process):
    deadline = time.monotonic() + 60
    while time.monotonic() < deadline:
        result = subprocess.run(['xdotool', 'search', '--onlyvisible', '--name', title],
                                capture_output=True, text=True)
        if result.returncode == 0:
            return result.stdout.splitlines()[-1]
        assert process.poll() in (None, 0), f'process exited early: {process.returncode}'
        time.sleep(0.1)
    raise TimeoutError(f'window did not appear: {title}')


def wait_closed(wid):
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        if subprocess.run(['xdotool', 'getwindowname', wid], capture_output=True).returncode != 0:
            return
        time.sleep(0.1)
    raise TimeoutError('application window did not close normally')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    image = args.image.resolve()
    image.chmod(0o755)
    manifest = image.parent / 'SHA256SUMS.txt'
    digest = hashlib.sha256(image.read_bytes()).hexdigest()
    assert f'{digest}  {image.name}' in manifest.read_text().splitlines()
    args.report.parent.mkdir(parents=True, exist_ok=True)
    report = {'artifact': image.name, 'sha256': digest, 'success': False}
    wm = subprocess.Popen(['openbox'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    process = None
    try:
        with tempfile.TemporaryDirectory(prefix='VYNX setup ') as temporary:
            root = Path(temporary)
            destination = root / 'QR folder % dollar$'
            env = os.environ | {'HOME': str(root), 'XDG_DATA_HOME': str(root / 'data'),
                                'XDG_CONFIG_HOME': str(root / 'config')}
            process = subprocess.Popen([str(image)], env=env)
            wid = window('Установка · VYNX QR', process)
            run('xdotool', 'windowactivate', '--sync', wid)
            run('import', '-window', wid, str(args.report.with_suffix('.welcome.png')))
            run('xdotool', 'key', '--clearmodifiers', 'Return')
            run('xdotool', 'key', '--clearmodifiers', 'alt+d')
            run('xdotool', 'type', '--clearmodifiers', '--', str(destination))
            run('xdotool', 'key', '--clearmodifiers', 'Return')
            deadline = time.monotonic() + 120
            desktop = root / 'data/applications/vynx-qr-local.desktop'
            while not desktop.exists() and time.monotonic() < deadline:
                assert process.poll() is None
                time.sleep(0.1)
            assert desktop.exists(), 'installation never produced a desktop entry'
            installed = destination / 'vynx-qr.AppImage'
            assert hashlib.sha256(installed.read_bytes()).hexdigest() == digest
            run('desktop-file-validate', str(desktop))
            wid = window('Установка завершена · VYNX QR', process)
            run('import', '-window', wid, str(args.report.with_suffix('.finish.png')))
            run('xdotool', 'windowactivate', '--sync', wid)
            run('xdotool', 'key', '--clearmodifiers', 'alt+r')
            run('xdotool', 'key', '--clearmodifiers', 'Return')
            process.wait(timeout=30)
            assert process.returncode == 0
            # Exercise desktop Exec expansion (including % and $ in the path).
            process = subprocess.Popen(['gio', 'launch', str(desktop)], env=env)
            app_window = window('^VYNX QR', process)
            run('wmctrl', '-ic', hex(int(app_window)))
            wait_closed(app_window)
            process.wait(timeout=30)
            assert process.returncode == 0
            report['installed_hash_matches'] = True
            report['desktop_validated'] = True
            report['version'] = subprocess.check_output([str(installed), '--version'], env=env, text=True).strip()
            foreign = destination / 'keep.txt'
            foreign.write_text('preserve unrelated files')
            process = subprocess.Popen(['/usr/bin/python3', '-c',
                'from gi.repository import Gio; import sys; '
                'Gio.DesktopAppInfo.new_from_filename(sys.argv[1]).launch_action("Uninstall", None)',
                str(desktop)], env=env)
            wid = window('Удаление VYNX QR', process)
            run('xdotool', 'windowactivate', '--sync', wid)
            run('xdotool', 'key', '--clearmodifiers', 'alt+y')
            wid = window('^VYNX QR$', process)
            run('xdotool', 'windowactivate', '--sync', wid)
            run('xdotool', 'key', '--clearmodifiers', 'Return')
            process.wait(timeout=30)
            assert process.returncode == 0
            assert not installed.exists() and not desktop.exists()
            assert foreign.read_text() == 'preserve unrelated files'
            report['uninstall_preserved_foreign_files'] = True
            report['success'] = True
    finally:
        if process and process.poll() is None:
            process.terminate()
            process.wait(timeout=10)
        wm.terminate()
        wm.wait(timeout=10)
        args.report.write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
