"""Generate exact NSIS removal instructions from the shipped payload inventory."""
import argparse
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--payload', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    files = [args.payload / name for name in ('VYNX QR.exe', 'LICENSE', 'THIRD_PARTY_LICENSES.md')]
    files += list(args.payload.glob('Qt6*.dll'))
    for name in ('licenses', 'platforms', 'styles'):
        files += [p for p in (args.payload / name).rglob('*') if p.is_file()]
    relative = sorted({p.relative_to(args.payload) for p in files})
    directories = {parent for p in relative for parent in p.parents if parent != Path('.')}
    def escaped(path):
        return str(path).replace('/', '\\').replace('$', '$$').replace('"', '$\\"')
    lines = [f'  Delete "$INSTDIR\\{escaped(p)}"' for p in relative]
    lines += [f'  RMDir "$INSTDIR\\{escaped(p)}"' for p in sorted(directories, key=lambda p: (-len(p.parts), str(p)))]
    args.output.write_text('\n'.join(lines) + '\n', encoding='utf-8-sig')


if __name__ == '__main__':
    main()
