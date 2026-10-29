"""Build an isolated V1.86 host with a regular main window and opt-in render capture."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

COMMIT = "02a817a069bac6bf4d263b5209d9c1b07fe2f950"


def replace_once(path, before, after):
    encoding = "utf-16" if path.read_bytes()[:2] in (b"\xff\xfe", b"\xfe\xff") else "utf-8-sig"
    text = path.read_text(encoding=encoding)
    if text.count(before) != 1:
        raise RuntimeError(f"Expected one source anchor in {path}: {before}")
    path.write_text(text.replace(before, after), encoding=encoding)


def record_provenance(output, evidence):
    (output / "source-provenance.json").write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
    fixture_record = output / "fixture/provenance.json"
    fixture = json.loads(fixture_record.read_text(encoding="utf-8"))
    fixture.update({"test_exe_sha256": evidence["host_sha256"], "mutex_isolated": True, "source_built": True, "source_commit": COMMIT, "source_provenance": "../source-provenance.json"})
    fixture_record.write_text(json.dumps(fixture, indent=2) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--upstream", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--dll", type=Path, required=True)
    args = parser.parse_args()
    upstream, output = args.upstream.resolve(), args.output.resolve()
    if output.exists() and any(output.iterdir()):
        raise SystemExit("Output must be new or empty")
    if subprocess.check_output(["git", "-C", str(upstream), "rev-parse", "HEAD"], text=True).strip() != COMMIT:
        raise SystemExit("Upstream checkout must be pinned to TrafficMonitor V1.86")
    output.mkdir(parents=True, exist_ok=True)
    source = output / "source"
    subprocess.run(["git", "clone", "--shared", str(upstream), str(source)], check=True)
    project = source / "TrafficMonitor"
    replace_once(project / "TrafficMonitor.cpp", "TrafficMonitor-1419J3XLKL1w8OZc", "NetWatch-SourceHost-v186")
    replace_once(project / "stdafx.h", "TrafficMonitor_r7XZaS4p", "NetWatchSourceHost_v186")
    replace_once(project / "TrafficMonitor.rc", "TrafficMonitor_r7XZaS4p", "NetWatchSourceHost_v186")
    replace_once(project / "TrafficMonitorDlg.cpp", "ModifyStyleEx(WS_EX_APPWINDOW, WS_EX_TOOLWINDOW);", "ModifyStyleEx(WS_EX_TOOLWINDOW, WS_EX_APPWINDOW);")
    replace_once(project / "TrafficMonitorDlg.cpp", '#include "TrafficMonitorDlg.h"', '#include "TrafficMonitorDlg.h"\n#include "NetWatchIntegration.h"')
    replace_once(project / "TrafficMonitorDlg.cpp", "void CTrafficMonitorDlg::OnTimer(UINT_PTR nIDEvent)\n{", "void CTrafficMonitorDlg::OnTimer(UINT_PTR nIDEvent)\n{\n    NetWatchIntegrationTick(*this);")
    shutil.copy2(Path(__file__).with_name("integration_capture.h"), project / "NetWatchIntegration.h")
    replace_once(project / "TrafficMonitor.vcxproj", '<Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets" />', '<ItemDefinitionGroup><ClCompile><AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions></ClCompile></ItemDefinitionGroup>\n  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets" />')
    (project / "compile_time.txt").write_text("NetWatch integration build\n", encoding="ascii")
    subprocess.run([sys.executable, str(Path(__file__).with_name("prepare_portable.py")), "--dll", str(args.dll.resolve()), "--output", str(output / "fixture"), "--archive", str(args.archive.resolve())], check=True)
    portable = output / "fixture" / "TrafficMonitor"
    config = portable / "config.ini"
    text = config.read_text(encoding="utf-8-sig").replace("[config]\n", "[config]\nalways_on_top=false\nhide_main_wnd_when_fullscreen=false\nposition_x=100\nposition_y=100\ntransparency=100\n")
    config.write_text(text, encoding="utf-8-sig")
    (portable / "NetWatch.capture").write_text("Capture the taskbar once after eight seconds.\n", encoding="ascii")
    vswhere = Path(r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe")
    installation = subprocess.check_output([str(vswhere), "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip()
    msbuild = Path(installation) / "MSBuild/Current/Bin/MSBuild.exe"
    command = [str(msbuild), str(project / "TrafficMonitor.vcxproj"), "/m:2", "/v:minimal", "/p:Configuration=Release (lite)", "/p:Platform=x64", "/p:PlatformToolset=v145", "/p:PreBuildEventUseInBuild=false", "/p:PostBuildEventUseInBuild=false", f"/p:OutDir={portable.as_posix()}/", f"/p:IntDir={(output / 'obj').as_posix()}/"]
    with (output / "build.log").open("w", encoding="utf-8") as log:
        result = subprocess.run(command, cwd=project, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        raise SystemExit(f"Upstream host build failed; see {output / 'build.log'}")
    evidence = {"commit": COMMIT, "host_sha256": hashlib.sha256((portable / "TrafficMonitor.exe").read_bytes()).hexdigest(), "plugin_sha256": hashlib.sha256((portable / "plugins/NetWatch.dll").read_bytes()).hexdigest(), "changes": ["unique mutex", "unique window class in registration and dialog resource", "regular main-window style", "opt-in timer capture of live taskbar renderer and tooltip control", "explicit UTF-8 compiler option"], "msbuild_command": command}
    record_provenance(output, evidence)
    with (output / "upstream.patch").open("wb") as patch:
        subprocess.run(["git", "-C", str(source), "diff"], stdout=patch, check=True)
    print(portable / "TrafficMonitor.exe")


if __name__ == "__main__":
    main()
