#!/usr/bin/env python3
"""Read the product version from CMake's project declaration, without a compiler."""
from pathlib import Path
import re


def project_version(root=None):
    root = Path(root) if root else Path(__file__).resolve().parents[1]
    text = (root / 'CMakeLists.txt').read_text(encoding='utf-8')
    match = re.search(r'project\(VYNX_QR\s+VERSION\s+(\d+\.\d+\.\d+)\s', text)
    if not match:
        raise RuntimeError('Could not read the literal product VERSION from project(VYNX_QR)')
    return match.group(1)


if __name__ == '__main__':
    print(project_version())
