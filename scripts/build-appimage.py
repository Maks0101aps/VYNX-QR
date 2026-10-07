#!/usr/bin/env python3
"""Deploy official Qt using checksum-pinned linuxdeploy and build an AppImage."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
QT_VERSION = '6.8.3'
QT_SOURCES = {
    'qtbase': '56001b905601bb9023d399f3ba780d7fa940f3e4861e496a7c490331f49e0b80',
    'qtwayland': '20fe385887d21190165a3180c17dcfc8b9a0e1da4ec76865b6334bdc709994b0',
    'qtsvg': '35eb516460f00f264eb504baa253432384351cf23fb9980a5857190e8deef438',
}


def run(*args, **kwargs):
    return subprocess.run(args, check=True, **kwargs)


def download(name, pin, cache):
    destination = cache / name
    if not destination.is_file() or hashlib.sha256(destination.read_bytes()).hexdigest() != pin['sha256']:
        headers = {'Accept': 'application/octet-stream'} if '/releases/assets/' in pin['url'] else {}
        request = urllib.request.Request(pin['url'], headers=headers)
        with urllib.request.urlopen(request, timeout=120) as response:
            data = response.read()
        assert hashlib.sha256(data).hexdigest() == pin['sha256'], f'download hash mismatch: {name}'
        destination.write_bytes(data)
    return destination


def archive_notice(archive, basename, destination):
    with tarfile.open(archive) as source:
        matches = [m for m in source.getmembers() if m.isfile() and Path(m.name).name == basename]
        assert matches, f'no {basename} in {archive}'
        # Keep every original notice if a source archive has several copies.
        for index, member in enumerate(matches):
            target = destination / f'{archive.name}-{index}-{basename}'
            target.write_bytes(source.extractfile(member).read())


def system_notices(appdir, cache, qt):
    notices = appdir / 'usr/share/doc/vynx-qr/licenses/system'
    notices.mkdir(parents=True)
    ldconfig = subprocess.check_output(['ldconfig', '-p'], text=True)
    system_paths = {}
    for line in ldconfig.splitlines():
        if '=>' in line:
            name = line.strip().split()[0]
            path = line.split('=>', 1)[1].strip()
            if 'x86-64' in line:
                system_paths[name] = path
    packages = {}
    qt_libraries = set()
    icu_libraries = set()
    for file in sorted((appdir / 'usr/lib').rglob('*')):
        if not file.is_file() or file.read_bytes()[:4] != b'\x7fELF':
            continue
        if file.name.startswith('libQt6'):
            qt_libraries.add(file.name)
            assert not re.search(r'Qt6(Qml|Quick|WebEngine|WaylandCompositor|Designer)', file.name), file.name
            continue
        if re.fullmatch(r'libicu(data|i18n|uc)\.so\.73(?:\.2)?', file.name):
            assert (qt / 'lib' / file.name).is_file(), f'ICU not supplied by official SDK: {file.name}'
            icu_libraries.add(file.name)
            continue
        origin = system_paths.get(file.name)
        if not origin:
            # SONAME aliases may carry a fully versioned filename in the AppDir.
            candidates = [p for p in system_paths.values() if Path(p).resolve().name == file.name]
            assert candidates, f'cannot establish system-library provenance: {file.name}'
            origin = candidates[0]
        paths = [origin, str(Path(origin).resolve())]
        if paths[-1].startswith('/usr/lib/'):
            paths.append(paths[-1].removeprefix('/usr'))
        owner = None
        for candidate in dict.fromkeys(paths):
            query = subprocess.run(['dpkg-query', '-S', candidate], text=True, capture_output=True)
            if query.returncode == 0:
                owner = query.stdout.split(': ', 1)[0]
                break
        assert owner, f'no distro owner for {file.name}: {paths}'
        package = owner.split(':', 1)[0]
        if package in packages:
            packages[package]['libraries'].append(file.name)
            continue
        copyright_file = Path('/usr/share/doc') / package / 'copyright'
        content = copyright_file.read_bytes()
        (notices / f'{package}-copyright').write_bytes(content)
        common = {}
        for name in set(re.findall(r'/usr/share/common-licenses/([A-Za-z0-9.+-]+)', content.decode('utf-8'))):
            name = name.rstrip('.')
            license_text = (Path('/usr/share/common-licenses') / name).read_bytes()
            destination = notices / 'common-licenses'
            destination.mkdir(exist_ok=True)
            (destination / name).write_bytes(license_text)
            common[name] = hashlib.sha256(license_text).hexdigest()
        metadata = subprocess.check_output(['dpkg-query', '-W', '-f=${source:Package}\t${source:Version}', owner], text=True)
        source_name, source_version = metadata.split('\t')
        item = {'package': package, 'source': source_name, 'version': source_version,
                'libraries': [file.name], 'notice_sha256': hashlib.sha256(content).hexdigest(),
                'common_licenses': common, 'sources': {}}
        # Runtime exception permits the GCC runtime's binary distribution under
        # its stated terms. Other bundled LGPL libraries retain source here.
        text = content.decode('utf-8')
        if re.search(r'^License:.*LGPL', text, re.M) and 'GCC Runtime Library Exception' not in text:
            source_dir = cache / 'system-source' / package
            source_dir.mkdir(parents=True, exist_ok=True)
            run('apt-get', 'source', '--download-only', '--only-source', f'{source_name}={source_version}', cwd=source_dir)
            destination = notices / 'source' / package
            destination.mkdir(parents=True)
            for source in source_dir.iterdir():
                if source.is_file():
                    shutil.copy2(source, destination / source.name)
                    item['sources'][source.name] = hashlib.sha256(source.read_bytes()).hexdigest()
            assert any(n.endswith('.dsc') for n in item['sources']), f'no source description for {package}'
        packages[package] = item
    assert any(n.startswith('libQt6Widgets') for n in qt_libraries)
    sdk = []
    if icu_libraries:
        probe = "import ctypes,json; v=(ctypes.c_uint8*4)(); ctypes.CDLL('libicuuc.so.73').u_getVersion_73(v); print(json.dumps(list(v)))"
        version = json.loads(subprocess.check_output(['python3', '-c', probe], text=True,
                             env=os.environ | {'LD_LIBRARY_PATH': str(qt / 'lib')}))
        assert version == [73, 2, 0, 0], f'unexpected SDK ICU version: {version}'
        pin = {'url': 'https://github.com/unicode-org/icu/releases/download/release-73-2/icu4c-73_2-src.tgz',
               'sha256': '818a80712ed3caacd9b652305e01afc7fa167e6f2e94996da44b90c2ab604ce1'}
        archive = download('icu4c-73_2-src.tgz', pin, cache)
        destination = notices / 'sdk/icu'
        destination.mkdir(parents=True)
        shutil.copy2(archive, destination / archive.name)
        archive_notice(archive, 'LICENSE', destination)
        sdk.append({'name': 'ICU', 'version': version, 'libraries': sorted(icu_libraries),
                    'files': {p.relative_to(notices).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                              for p in destination.iterdir() if p.is_file()}, 'source': pin})
    (notices / 'manifest.json').write_text(json.dumps({'libraries': sorted(qt_libraries),
        'packages': list(packages.values()), 'sdk_dependencies': sdk}, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary-dir', type=Path, required=True)
    parser.add_argument('--qt-prefix', type=Path, required=True)
    args = parser.parse_args()
    assert platform.system() == 'Linux' and platform.machine() == 'x86_64', 'native Linux x86_64 packaging required'
    build = args.binary_dir.resolve()
    qt = args.qt_prefix.resolve()
    qmake = qt / 'bin/qmake'
    assert subprocess.check_output([str(qmake), '-query', 'QT_VERSION'], text=True).strip() == QT_VERSION
    spec = importlib.util.spec_from_file_location('project_version', ROOT / 'scripts/project-version.py')
    version_module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(version_module)
    version = version_module.project_version(ROOT)
    tools_dir = build / 'appimage-tools'
    tools_dir.mkdir(parents=True, exist_ok=True)
    pins = json.loads((ROOT / 'packaging/appimage/tools.json').read_text())
    tools = {name: download(name, pin, tools_dir) for name, pin in pins.items()}
    for name in ('linuxdeploy-x86_64.AppImage', 'linuxdeploy-plugin-qt-x86_64.AppImage', 'runtime-x86_64'):
        tools[name].chmod(0o755)
    appdir = build / 'AppDir'
    assert appdir.resolve().parent == build and not appdir.is_symlink(), 'unsafe staging path'
    if appdir.exists():
        shutil.rmtree(appdir)
    run('cmake', '--install', str(build), env=os.environ | {'DESTDIR': str(appdir)})
    plugin_dir = qt / 'plugins/platforms'
    wayland_plugins = sorted(p.name for p in plugin_dir.glob('libqwayland*.so'))
    assert wayland_plugins, 'official Qt SDK has no Wayland platform plugin'
    env = os.environ | {'APPIMAGE_EXTRACT_AND_RUN': '1', 'QMAKE': str(qmake),
                        # CMake strips build RPATH during installation. Resolve
                        # the official SDK while deploying the installed ELF.
                        'LD_LIBRARY_PATH': str(qt / 'lib') + (':' + os.environ['LD_LIBRARY_PATH'] if os.environ.get('LD_LIBRARY_PATH') else ''),
                        'EXTRA_PLATFORM_PLUGINS': ';'.join(wayland_plugins),
                        # This deployer copies client integration directories;
                        # the audit rejects an actual compositor library.
                        'EXTRA_QT_MODULES': 'waylandcompositor',
                        'LDAI_RUNTIME_FILE': str(tools['runtime-x86_64']),
                        'LDAI_OUTPUT': str(build / 'package' / f'VYNX-QR-{version}-x86_64.AppImage'),
                        'LDAI_VERSION': version, 'LDAI_NO_APPSTREAM': '1'}
    deploy = str(tools['linuxdeploy-x86_64.AppImage'])
    run(deploy, '--appdir', str(appdir), '--plugin', 'qt', cwd=tools_dir, env=env)
    assert (appdir / 'usr/plugins/platforms/libqxcb.so').is_file()
    assert list((appdir / 'usr/plugins/platforms').glob('libqwayland*.so'))
    system_notices(appdir, tools_dir, qt)
    qt_docs = appdir / 'usr/share/doc/vynx-qr/licenses/qt'
    qt_manifest = {}
    for module, digest in QT_SOURCES.items():
        name = f'{module}-everywhere-src-{QT_VERSION}.tar.xz'
        pin = {'url': f'https://download.qt.io/archive/qt/6.8/{QT_VERSION}/submodules/{name}', 'sha256': digest}
        source = download(name, pin, tools_dir)
        shutil.copy2(source, qt_docs / name)
        qt_manifest[name] = pin
    (qt_docs / 'manifest.json').write_text(json.dumps(qt_manifest, indent=2) + '\n')
    shutil.copy2(ROOT / 'packaging/appimage/README-Qt.md', qt_docs / 'README.md')
    runtime_docs = appdir / 'usr/share/doc/vynx-qr/licenses/appimage-runtime'
    runtime_docs.mkdir(parents=True)
    runtime_manifest = {}
    for name, source in tools.items():
        if name.endswith('.AppImage') or name == 'runtime-x86_64':
            continue
        shutil.copy2(source, runtime_docs / name)
        runtime_manifest[name] = pins[name]
    archive_notice(tools['runtime-source.tar.gz'], 'LICENSE', runtime_docs)
    archive_notice(tools['fuse-3.15.0.tar.xz'], 'LGPL2.txt', runtime_docs)
    archive_notice(tools['squashfuse-0.5.2.tar.gz'], 'LICENSE', runtime_docs)
    (runtime_docs / 'manifest.json').write_text(json.dumps(runtime_manifest, indent=2) + '\n')
    run('python3', str(ROOT / 'scripts/verify-rust-notices.py'), str(appdir / 'usr/share/doc/vynx-qr/licenses/rust'),
        '--target', 'x86_64-unknown-linux-gnu', '--lockfile', str(ROOT / 'bridge/Cargo.lock'))
    (build / 'package').mkdir(exist_ok=True)
    libraries = {p.relative_to(appdir).as_posix() for p in (appdir / 'usr/lib').rglob('*') if p.is_file()}
    run(deploy, '--appdir', str(appdir), '--output', 'appimage', cwd=tools_dir, env=env)
    assert libraries == {p.relative_to(appdir).as_posix() for p in (appdir / 'usr/lib').rglob('*') if p.is_file()}, 'output plugin changed audited library inventory'
    image = Path(env['LDAI_OUTPUT'])
    assert image.is_file()
    inventory = [p.relative_to(appdir).as_posix() for p in appdir.rglob('*') if p.is_file()]
    (build / 'appimage-inventory.json').write_text(json.dumps(sorted(inventory), indent=2) + '\n')
    print(f'AppImage: {image}, {image.stat().st_size} bytes')


if __name__ == '__main__':
    main()
