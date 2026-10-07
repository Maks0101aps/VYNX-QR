#!/usr/bin/env python3
"""Exercise a real X11 window; retain metrics and diagnostics on failure."""
import argparse
from contextlib import nullcontext
import json
import os
from pathlib import Path
import statistics
import subprocess
import tempfile
import time


def command(*args):
    return subprocess.check_output(args, text=True).strip()


def mount_helpers(runtime):
    helpers = []
    for proc in Path('/proc').iterdir():
        if not proc.name.isdecimal():
            continue
        try:
            if (proc / 'exe').resolve() == runtime:
                status = (proc / 'status').read_text()
                rss = next(line for line in status.splitlines() if line.startswith('VmRSS:'))
                helpers.append({'pid': int(proc.name), 'executable': str(runtime),
                                'VmRSS_KiB': int(rss.split()[1])})
        except (FileNotFoundError, PermissionError, ProcessLookupError, StopIteration):
            continue
    return helpers


def sample(pid, runtime=None):
    proc = Path('/proc') / str(pid)
    status = dict(line.split(':', 1) for line in (proc / 'status').read_text().splitlines())
    memory = {}
    for name in ('VmRSS', 'VmHWM'):
        memory[name + '_KiB'] = int(status[name].split()[0])
    if (proc / 'smaps_rollup').exists():
        for line in (proc / 'smaps_rollup').read_text().splitlines():
            if line.startswith('Pss:'):
                memory['Pss_KiB'] = int(line.split()[1])
    children = (proc / 'task' / str(pid) / 'children').read_text().split()
    helpers = []
    for child in children:
        executable = (Path('/proc') / child / 'exe').resolve()
        assert runtime and executable == runtime, f'application has unexpected child {child}: {executable}'
        helpers.append({'pid': int(child), 'executable': str(executable)})
    memory['children'] = len(children)
    if runtime:
        # The FUSE daemon normally reparents to PID 1, so child count alone
        # would conceal its existence. Identify it by the pinned image file.
        memory['appimage_mount_helpers'] = mount_helpers(runtime)
        appdir = (proc / 'exe').resolve().parents[2]
        qt_paths = sorted({line.split(maxsplit=5)[-1] for line in (proc / 'maps').read_text().splitlines()
                           if '/libQt6' in line})
        assert len(qt_paths) >= 3, 'Qt runtime mapping missing'
        assert all(Path(path).is_relative_to(appdir) for path in qt_paths), f'Qt loaded outside AppImage: {qt_paths}'
        memory['loaded_qt_libraries'] = qt_paths
    fields = (proc / 'stat').read_text().rsplit(')', 1)[1].split()
    memory['cpu_ticks'] = int(fields[11]) + int(fields[12])
    return memory


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('executable')
    parser.add_argument('--report', required=True)
    parser.add_argument('--runs', type=int, default=5)
    parser.add_argument('--config-home', type=Path)
    parser.add_argument('--appimage', action='store_true', help='Record the AppImage runtime mount helper separately')
    parser.add_argument('--screenshot', type=Path, help='Capture the first real window after entering a sample URL')
    args = parser.parse_args()
    report_path = Path(args.report)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report = {'platform': 'X11/Xvfb', 'samples': [], 'success': False}
    wm = None
    try:
        executable = str(Path(args.executable).resolve())
        version = command(executable, '--version')
        assert version.startswith('VYNX QR '), version
        assert 'Usage:' in command(executable, '--help')
        report['version'] = version
        wm = subprocess.Popen(['openbox'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        # Openbox publishes its manager identity before its client list. Wait
        # for both before mapping the application into the new display.
        deadline = time.monotonic() + 10
        while True:
            assert wm.poll() is None, 'window manager exited during startup'
            manager = subprocess.run(['wmctrl', '-m'], text=True, capture_output=True)
            clients = subprocess.run(['wmctrl', '-lp'], text=True, capture_output=True)
            if manager.returncode == 0 and clients.returncode == 0:
                break
            assert time.monotonic() < deadline, 'window manager/client list did not become ready'
            time.sleep(.1)
        config_context = (nullcontext(str(args.config_home.resolve())) if args.config_home
                          else tempfile.TemporaryDirectory(prefix='vynx smoke '))
        with config_context as config:
            env = os.environ | {'XDG_CONFIG_HOME': config, 'QT_QPA_PLATFORM': 'xcb'}
            for index in range(args.runs):
                log_path = report_path.with_suffix(f'.{index}.stderr.log')
                with log_path.open('w') as log:
                    started = time.monotonic()
                    app = subprocess.Popen([executable], env=env, stdout=log, stderr=log)
                    try:
                        deadline = started + 20
                        window = None
                        while not window:
                            assert app.poll() is None, f'application exited: {app.returncode}; see {log_path}'
                            clients = subprocess.run(['wmctrl', '-lp'], text=True, capture_output=True)
                            # Openbox exposes its manager identity before creating
                            # _NET_CLIENT_LIST. No mapped clients is a startup state,
                            # not a failed application. Other wmctrl errors stay fatal.
                            if clients.returncode:
                                assert 'Cannot get client list properties' in clients.stderr, clients.stderr
                                report['client_list_pending_observations'] = report.get('client_list_pending_observations', 0) + 1
                            for row in clients.stdout.splitlines():
                                fields = row.split(None, 4)
                                if len(fields) == 5 and fields[2] == str(app.pid) and 'VYNX QR' in fields[4]:
                                    window = fields[0]
                                    break
                            if time.monotonic() >= deadline:
                                report['window_timeout_diagnostics'] = {
                                    'app_pid': app.pid,
                                    'wm_pid': wm.pid,
                                    'wm_exit_code': wm.poll(),
                                    'clients_stdout': clients.stdout,
                                    'clients_stderr': clients.stderr,
                                    'app_status': (Path('/proc') / str(app.pid) / 'status').read_text(),
                                    'app_wait_channel': (Path('/proc') / str(app.pid) / 'wchan').read_text(),
                                }
                                raise AssertionError('no application window')
                            if not window:
                                time.sleep(.05)
                        elapsed = (time.monotonic() - started) * 1000
                        before = sample(app.pid, Path(executable) if args.appimage else None)
                        idle_started = time.monotonic()
                        time.sleep(1)
                        after = sample(app.pid, Path(executable) if args.appimage else None)
                        after['idle_cpu_percent'] = 100 * (after['cpu_ticks'] - before['cpu_ticks']) / os.sysconf('SC_CLK_TCK') / (time.monotonic() - idle_started)
                        after['startup_ms'] = elapsed
                        report['samples'].append(after)
                        if index == 0 and args.screenshot:
                            args.screenshot.parent.mkdir(parents=True, exist_ok=True)
                            subprocess.run(['xdotool', 'windowactivate', '--sync', window], check=True)
                            subprocess.run(['xdotool', 'type', '--clearmodifiers', 'github.com'], check=True)
                            time.sleep(1)
                            subprocess.run(['import', '-window', window, str(args.screenshot)], check=True)
                            report['screenshot'] = str(args.screenshot)
                        # A window-manager close exercises the normal Qt close path.
                        subprocess.run(['wmctrl', '-ic', window], check=True)
                        assert app.wait(timeout=10) == 0, 'application did not close cleanly'
                        if args.appimage:
                            deadline = time.monotonic() + 10
                            while mount_helpers(Path(executable)):
                                assert time.monotonic() < deadline, 'AppImage mount helper survived close'
                                time.sleep(.05)
                            report['mount_helpers_removed_after_close'] = True
                        report['settings_path'] = str(Path(config) / 'VYNX/QR/settings.json')
                    finally:
                        if app.poll() is None:
                            app.terminate()
                            app.wait(timeout=10)
            report['first_startup_ms'] = report['samples'][0]['startup_ms']
            report['warm_median_startup_ms'] = statistics.median(s['startup_ms'] for s in report['samples'][1:] or report['samples'])
        report['success'] = True
    except Exception as error:
        report['error'] = str(error)
        raise
    finally:
        if wm:
            wm.terminate()
            wm.wait(timeout=10)
        report_path.write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
