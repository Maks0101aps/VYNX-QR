"""Generate the release Rust notice bundle from the locked Windows dependency graph."""

import argparse
import hashlib
import html
import io
import json
from pathlib import Path
import subprocess
import tarfile
import urllib.request

VERSION = "0.9.2"
TOOL_SHA256 = "1c03e5890238562497c2d89a3b75b02560af349c1fc3e713d3284f532a5cd748"
ROOT = Path(__file__).resolve().parents[1]


def cargo_about(cache):
    executable = cache / "cargo-about.exe"
    archive = cache / "cargo-about.tar.gz"
    cache.mkdir(parents=True, exist_ok=True)
    if not archive.exists() or hashlib.sha256(archive.read_bytes()).hexdigest() != TOOL_SHA256:
        url = (f"https://github.com/EmbarkStudios/cargo-about/releases/download/{VERSION}/"
               f"cargo-about-{VERSION}-x86_64-pc-windows-msvc.tar.gz")
        with urllib.request.urlopen(url, timeout=120) as response:
            data = response.read()
        if hashlib.sha256(data).hexdigest() != TOOL_SHA256:
            raise RuntimeError("cargo-about download SHA256 mismatch")
        archive.write_bytes(data)
    # Read only the executable; no archive-controlled paths are extracted.
    with tarfile.open(archive) as package:
        members = [m for m in package.getmembers() if m.isfile() and m.name.endswith("/cargo-about.exe")]
        if len(members) != 1:
            raise RuntimeError("Expected one cargo-about executable")
        data = package.extractfile(members[0]).read()
    if not executable.exists() or executable.read_bytes() != data:
        executable.write_bytes(data)
    return executable


def generate(output, cache):
    tool = cargo_about(cache)
    subprocess.run([
        str(tool), "generate", "--locked", "--fail", "-c", str(ROOT / "about.toml"),
        "-m", str(ROOT / "bridge/Cargo.toml"), "--format", "json", "-o", str(cache / "about.json"),
    ], cwd=ROOT, check=True)
    about = json.loads((cache / "about.json").read_text(encoding="utf-8"))
    crates = sorted((c["package"] for c in about["crates"]), key=lambda c: (c["name"], c["version"]))
    covered = {u["crate"]["id"] for license in about["licenses"] for u in license["used_by"]}
    if covered != {c["id"] for c in crates}:
        raise RuntimeError("cargo-about licence coverage does not match the crate inventory")
    output.mkdir(parents=True, exist_ok=True)
    inventory = []
    sections = []
    for crate in crates:
        key = f"{crate['name']}-{crate['version']}"
        source = Path(crate["manifest_path"]).parent
        files = sorted(p for p in source.rglob("*") if p.is_file() and
                       p.name.upper().startswith(("LICENSE", "LICENCE", "NOTICE", "COPYING", "COPYRIGHT")))
        if crate.get("license_file"):
            files = sorted(set(files) | {source / crate["license_file"]})
        if not files:
            raise RuntimeError(f"No original licence/copyright files for {key}")
        notices = []
        hashes = {}
        for file in files:
            relative = file.relative_to(source)
            destination = output / "notices" / key / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            data = file.read_bytes()
            destination.write_bytes(data)
            hashes[relative.as_posix()] = hashlib.sha256(data).hexdigest()
            notices.append(f"<h3>{html.escape(relative.as_posix())}</h3><pre>"
                           f"{html.escape(data.decode('utf-8'))}</pre>")
        selected = sorted({license["id"] for license in about["licenses"]
                           if any(u["crate"]["id"] == crate["id"] for u in license["used_by"])})
        inventory.append({"name": crate["name"], "version": crate["version"],
                          "license_expression": crate["license"], "selected_licenses": selected,
                          "authors": crate["authors"], "notice_sha256": hashes})
        sections.append(f"<section id='{html.escape(key)}'><h2>{html.escape(key)}</h2>"
                        f"<p>{html.escape(crate['license'])}; selected: {html.escape(', '.join(selected))}</p>"
                        f"<p>Authors: {html.escape(', '.join(crate['authors']))}</p>"
                        + "".join(notices) + "</section>")
    # Keep cargo-about's resolved licence texts as well as the unaltered files above.
    licenses = "".join(f"<h2>{html.escape(l['id'])}</h2><pre>{html.escape(l['text'])}</pre>"
                       for l in about["licenses"])
    lock_hash = hashlib.sha256((ROOT / "bridge/Cargo.lock").read_bytes()).hexdigest()
    manifest = {"generator": f"cargo-about {VERSION}", "target": "x86_64-pc-windows-msvc",
                "lockfile_sha256": lock_hash, "crates": inventory}
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    content = ("<!doctype html><html lang='en'><meta charset='utf-8'>"
               "<title>VYNX QR Rust third party licences</title>"
               "<style>body{max-width:960px;margin:32px auto;padding:0 20px;font-family:system-ui}"
               "pre{white-space:pre-wrap;overflow-wrap:anywhere}section{border-top:1px solid #aaa}</style>"
               "<h1>Rust third party licences and copyright notices</h1>"
               f"<p>Generated with cargo-about {VERSION} from bridge/Cargo.lock ({lock_hash}). "
               f"{len(crates)} crates in the default-feature Windows release dependency graph. "
               "Build-only and development-only dependencies are excluded. This conservative "
               "inventory also retains production procedural macro dependencies; it is not "
               "a claim that every crate contributes machine code after linker optimisation.</p>"
               "<p>Original upstream licence and notice files are reproduced below and are also "
               "included unmodified under notices/. The application licence does not restrict "
               "rights granted by these licences.</p>" + "".join(sections)
               + "<h1>Resolved licence texts from cargo-about</h1>" + licenses + "</html>\n")
    (output / "THIRD_PARTY_RUST_LICENSES.html").write_text(content, encoding="utf-8")
    print(f"Rust licence bundle: {len(crates)} crates, {sum(len(c['notice_sha256']) for c in inventory)} original files")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cache", type=Path, required=True)
    args = parser.parse_args()
    generate(args.output.resolve(), args.cache.resolve())
