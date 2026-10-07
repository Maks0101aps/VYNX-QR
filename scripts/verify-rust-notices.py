#!/usr/bin/env python3
"""Verify the packaged, target-specific upstream Rust notice inventory."""
import argparse
import hashlib
import json
from pathlib import Path


def verify(root, target, lockfile=None):
    root = root.resolve()
    manifest = json.loads((root / 'manifest.json').read_text(encoding='utf-8'))
    assert manifest['target'] == target, 'wrong dependency target'
    assert (root / 'THIRD_PARTY_RUST_LICENSES.html').is_file(), 'missing readable licences'
    if lockfile:
        assert manifest['lockfile_sha256'] == hashlib.sha256(lockfile.read_bytes()).hexdigest(), 'wrong source lockfile'
    names = {c['name'] for c in manifest['crates']}
    assert {'qrcode', 'rqrr', 'image', 'serde', 'cxx'} <= names
    assert not {'tempfile', 'cxx-build', 'cc'} & names, 'build/development dependencies included'
    if target.endswith('linux-gnu'):
        assert not {n for n in names if n.startswith('windows')}, 'Windows graph in Linux package'
    expected = set()
    for crate in manifest['crates']:
        assert crate['notice_sha256'], f"no notices for {crate['name']}"
        assert crate['selected_licenses'], f"no resolved licence for {crate['name']}"
        for name, digest in crate['notice_sha256'].items():
            file = (root / 'notices' / f"{crate['name']}-{crate['version']}" / name).resolve()
            assert file.is_relative_to(root / 'notices'), 'notice path escapes bundle'
            assert hashlib.sha256(file.read_bytes()).hexdigest() == digest, f'notice hash mismatch: {file}'
            expected.add(file)
    actual = {p.resolve() for p in (root / 'notices').rglob('*') if p.is_file()}
    assert actual == expected, 'stale or unlisted notice files'
    result = {'target': target, 'crates': len(manifest['crates']), 'notice_files': len(expected), 'verified': True}
    print(json.dumps(result))
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', type=Path)
    parser.add_argument('--target', required=True)
    parser.add_argument('--lockfile', type=Path)
    args = parser.parse_args()
    verify(args.root, args.target, args.lockfile)
