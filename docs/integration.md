# Additional TrafficMonitor integration checks

These optional checks build and run a **modified disposable upstream test host**. They do not install NetWatch or change an existing TrafficMonitor. Keep all outputs under ignored `.work/`.

## Prepare a source-built host

Requirements beyond the normal plugin build: Visual Studio 2026, the v145 toolset and C++ MFC components. The script currently targets that installed toolchain explicitly.

Obtain the official V1.86 source checkout at commit `02a817a069bac6bf4d263b5209d9c1b07fe2f950` and the pinned `TrafficMonitor_V1.86_x64_Lite.zip` described in verification.md. Then run:

```powershell
python tests/prepare_source_host.py `
  --upstream .work/TrafficMonitor `
  --output .work/source-integration `
  --archive .work/TrafficMonitor_V1.86_x64_Lite.zip `
  --dll build/Release/NetWatch.dll
```

The output directory must be new or empty. The script validates the source commit and release-archive hash, clones the source locally, applies explicit source anchors and builds the Lite configuration. The original checkout is read-only input. Upstream sources stay under their own Anti-996 license; no upstream implementation is vendored into NetWatch.

The fixture is under `fixture/TrafficMonitor`. `source-provenance.json` records binary hashes, changes and the build command; `upstream.patch` records tracked source edits; `build.log` records the upstream build, including its own warnings. The additional `NetWatchIntegration.h` contains only the test capture code. The plugin DLL is copied byte-for-byte from the requested build.

Launch only this fixture's executable. After eight seconds with `NetWatch.capture` present, the in-process test hook saves:

- `NetWatch-taskbar.png`: the current taskbar object's unmodified ShowInfo GDI renderer drawing into an offscreen DC at its actual client size.
- `NetWatch-tooltip.txt`: text already stored in the host's tooltip control. It includes host metrics as well as the plugin section; do not publish the entire file without reviewing it.
- `NetWatch-integration.txt`: dimensions, DPI, visibility, insertion failure flag and item IDs.

The regular main window is targetable by the computer-use helper. Use its context menu's Other Functions > Plug-in Manage to inspect NetWatch and open Options. Some helper versions return screenshots for the owner and each modal separately. Use the screenshot belonging to the intended modal; assuming the first screenshot represents that modal caused the earlier geometry failures. Coordinates and indexes must come from the current observation.

Changing the main-window style does not expose the embedded taskbar child as a separate automation target. The bitmap validates the live host's render path, not OS composition, occlusion or popup placement. Keep this distinction in the verification record.

## Exit during a held HTTP request

Start the controlled loopback fixture in another terminal:

```powershell
python tests/hold_http_fixture.py --state .work/http-state.json
```

It chooses a local port, records receipt times and holds response headers for up to 60 seconds. It stops after 180 seconds, or on a request to its own `/shutdown` endpoint. It never binds a public interface.

Configure one enabled HTTP check in the **disposable host** to `http://127.0.0.1:<recorded-port>/hold`, with timeout 30000 ms and interval 30000 ms. Start that host, verify the server has received a request and has completed no response, and select Exit through the host UI before the 30-second budget expires.

`tests/watch_process_exit.py` can hold a read-only process handle before the click:

```powershell
python tests/watch_process_exit.py `
  --pid <disposable-host-pid> `
  --executable .work/source-integration/fixture/TrafficMonitor/TrafficMonitor.exe `
  --result .work/exit.json
```

It verifies the PID's full executable path, observes for up to 45 seconds, and records the raw exit code and timestamp. It does not send window messages, input, termination signals or configuration changes. Compare the exit timestamp with the server's recorded request time and require request age below the configured timeout. Merely observing a server handler still waiting is insufficient: the client may already have timed out.

Use an otherwise identical host with NetWatch disabled as the exit-code control. TrafficMonitor's MFC modal shutdown returned code 2 in both observed cases, so zero must not be assumed to be its normal GUI exit code. The native isolated DLL tests separately verify unloading and callback cleanup; a GUI process-exit test does not prove all work completed gracefully.

Only normal exit via the test host's own UI counts for this check. Force-stopping a test process is cleanup, not positive exit evidence.
