#!/usr/bin/env python3
"""Stage the exact Git commit, render PKGBUILD metadata, optionally run makepkg."""
import argparse
import hashlib
import importlib.util
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('project_version', ROOT / 'scripts/project-version.py')
version_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(version_module)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--build', action='store_true')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    version = version_module.project_version(ROOT)
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    archive = output / 'vynx-qr-source.tar.gz'
    subprocess.run(['git', 'archive', 'HEAD', '--format=tar.gz', '--prefix=vynx-qr/', '-o', str(archive)], cwd=ROOT, check=True)
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    template = (ROOT / 'packaging/arch/PKGBUILD.in').read_text()
    (output / 'PKGBUILD').write_text(template.replace('@PROJECT_VERSION@', version).replace('@SOURCE_SHA256@', digest))
    (output / 'source-commit.txt').write_text(commit + '\n')
    print(f'PKGBUILD: {version}, source commit {commit}, SHA256 {digest}', flush=True)
    if args.build:
        subprocess.run(['makepkg', '--cleanbuild', '--noconfirm'], cwd=output, check=True)


if __name__ == '__main__':
    main()
