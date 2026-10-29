"""Prepare a disposable V1.86 portable host without touching installed copies."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import urllib.request
import uuid
import zipfile

URL = "https://github.com/zhongyang219/TrafficMonitor/releases/download/V1.86/TrafficMonitor_V1.86_x64_Lite.zip"
SHA256 = "d9774a64268f952b5cb88a1eead399f863f7d4ab64e945f19c096fed89cc1145"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dll", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--isolate-instance", action="store_true", help="Change only the portable EXE's instance-mutex name so existing TrafficMonitor processes can keep running")
    args = parser.parse_args()
    if args.output.exists() and any(args.output.iterdir()):
        raise SystemExit("Output directory must be new or empty")
    args.output.mkdir(parents=True, exist_ok=True)
    archive = args.archive or args.output / "TrafficMonitor_V1.86_x64_Lite.zip"
    if not archive.exists():
        urllib.request.urlretrieve(URL, archive)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != SHA256:
        raise SystemExit("Unexpected TrafficMonitor archive hash")
    with zipfile.ZipFile(archive) as package:
        root = args.output.resolve()
        for name in package.namelist():
            if not (root / name).resolve().is_relative_to(root):
                raise SystemExit("Archive path escapes output directory")
        package.extractall(args.output)
    host = args.output / "TrafficMonitor"
    exe = host / "TrafficMonitor.exe"
    original_hash = hashlib.sha256(exe.read_bytes()).hexdigest()
    if args.isolate_instance:
        data = exe.read_bytes()
        original = "TrafficMonitor-1419J3XLKL1w8OZc".encode("utf-16-le")
        replacement = "NetWatchHost---1419J3XLKL1w8OZc".encode("utf-16-le")
        assert len(original) == len(replacement)
        if data.count(original) != 1:
            raise SystemExit("Expected exactly one upstream mutex string")
        exe.write_bytes(data.replace(original, replacement))
    plugins = host / "plugins"
    plugins.mkdir(exist_ok=True)
    shutil.copy2(args.dll, plugins / "NetWatch.dll")
    groups = []
    checks = [
        [("Loopback", "127.0.0.1")],
        [("Loopback", "127.0.0.1"), ("Invalid name", "bad host name")],
        [("Invalid name", "bad host name")],
        [],
    ]
    lines = ["NetWatch 1", "max_visible All"]
    for label, members in zip(["UP", "MIX", "DOWN", "NEW"], checks):
        identity = str(uuid.uuid4())
        groups.append(identity)
        lines.append(f'group "{identity}" "{label} fixture" "{label}" 1 1')
        for name, target in members:
            lines.append(f'check "{uuid.uuid4()}" "{name}" 0 "{target}" 80 1000 500 1')
    (plugins / "NetWatch.conf").write_text("\n".join(lines) + "\n", encoding="utf-8")
    (host / "global_cfg.ini").write_text("[config]\nportable_mode=true\n", encoding="utf-8-sig")
    config = "[general]\nlanguage=1033\ncheck_update_when_start=false\n[config]\nshow_task_bar_wnd=true\nhide_main_window=false\nshow_notify_icon=false\n[task_bar]\ntbar_display_item=0\nplugin_display_item="
    config += ",".join("netwatch." + identity for identity in groups)
    config += "\ndisable_d2d=true\n"
    (host / "config.ini").write_text(config, encoding="utf-8-sig")
    evidence = {"url": URL, "archive_sha256": SHA256, "original_exe_sha256": original_hash, "test_exe_sha256": hashlib.sha256(exe.read_bytes()).hexdigest(), "mutex_isolated": args.isolate_instance}
    (args.output / "provenance.json").write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
    print(exe.resolve())
    print(json.dumps(evidence, indent=2))


if __name__ == "__main__":
    main()
