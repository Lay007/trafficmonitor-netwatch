"""Package a recorded, verified Release DLL without publishing or installing it."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
TAG = "v0.1.0-beta.1"


def digest(data):
    return hashlib.sha256(data).hexdigest()


def git(*args):
    return subprocess.check_output(["git", "-C", str(ROOT), *args]).decode().strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dll", type=Path, default=ROOT / "build/Release/NetWatch.dll")
    parser.add_argument("--output", type=Path, required=True, help="New output directory; existing paths are refused")
    args = parser.parse_args()
    if args.output.exists():
        raise SystemExit("Output must be new; do not overwrite a reviewed release")
    if git("diff", "HEAD", "--", "src", "vendor", "CMakeLists.txt"):
        raise SystemExit("Commit source changes before recording release provenance")
    verification = (ROOT / "docs/verification.md").read_text(encoding="utf-8")
    recorded = re.search(r"Current Release DLL SHA-256: `([0-9a-f]{64})`", verification)
    dll = args.dll.read_bytes()
    if not recorded or digest(dll) != recorded[1]:
        raise SystemExit("DLL hash does not match docs/verification.md; verify and record the intended binary first")
    pe = struct.unpack_from("<I", dll, 0x3C)[0]
    if dll[:2] != b"MZ" or dll[pe:pe+4] != b"PE\0\0" or struct.unpack_from("<H", dll, pe+4)[0] != 0x8664:
        raise SystemExit("Expected a Windows x64 PE binary")
    if not struct.unpack_from("<H", dll, pe+22)[0] & 0x2000:
        raise SystemExit("Expected a DLL")
    source_commit = git("rev-parse", "HEAD")
    version = re.search(r"project\(NetWatch VERSION ([0-9.]+)", (ROOT / "CMakeLists.txt").read_text())[1]
    plugin_source = (ROOT / "src/plugin.cpp").read_text(encoding="utf-8")
    if f'case TMI_VERSION: return L"{version}";' not in plugin_source:
        raise SystemExit("CMake and plugin versions differ")
    payload = {
        "NetWatch.dll": dll,
        "README.md": (ROOT / "packaging/README.md").read_bytes(),
        "RELEASE_NOTES.md": (ROOT / f"docs/releases/{TAG}.md").read_bytes(),
    }
    names = ["LICENSE", "THIRD_PARTY_NOTICES.md", "vendor/TrafficMonitor.LICENSE", "vendor/README.md",
             "docs/ssh-import.md", "docs/localization.md", "docs/verification.md", "docs/architecture.md",
             "docs/roadmap.md", "docs/integration.md", "docs/images/Taskbar-v186-gdi-150.png",
             "docs/evidence/tooltip-v186.txt"]
    payload.update({name: (ROOT / name).read_bytes() for name in names})
    manifest = {"release_tag": TAG, "prerelease": True, "plugin_version": version,
                "source_commit": source_commit, "target": "Windows x64", "trafficmonitor_version": "1.86",
                "plugin_api": 7, "configuration": "Release", "dll_sha256": digest(dll),
                "files": {name: digest(data) for name, data in sorted(payload.items())}}
    payload["manifest.json"] = (json.dumps(manifest, indent=2) + "\n").encode()
    args.output.mkdir(parents=True)
    archive = args.output / f"NetWatch-{TAG}-windows-x64.zip"
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as package:
        for name, data in sorted(payload.items()):
            info = zipfile.ZipInfo(name, date_time=(2026, 11, 3, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            package.writestr(info, data)
    with zipfile.ZipFile(archive) as package:
        if package.testzip() is not None or set(package.namelist()) != set(payload):
            raise SystemExit("Archive integrity/file-list verification failed")
        for name, expected in payload.items():
            if package.read(name) != expected:
                raise SystemExit(f"Packaged bytes differ: {name}")
    checksum = f"{digest(archive.read_bytes())}  {archive.name}\n"
    (args.output / "SHA256SUMS.txt").write_text(checksum, encoding="ascii")
    (args.output / "manifest.json").write_bytes(payload["manifest.json"])
    print(json.dumps({"archive": str(archive.resolve()), "sha256": digest(archive.read_bytes()),
                      "dll_sha256": digest(dll), "source_commit": source_commit, "files": len(payload)}, indent=2))


if __name__ == "__main__":
    main()
