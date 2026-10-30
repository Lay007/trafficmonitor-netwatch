# TrafficMonitor NetWatch

NetWatch is a Windows x64 C++ plugin for TrafficMonitor 1.86. It monitors user-defined groups of devices and services with ICMP, TCP and HTTP/HTTPS probes. Each selected group has a compact text label and a GDI-drawn status circle.

| Circle | Meaning |
| --- | --- |
| Green | All enabled checks are Up |
| Yellow | A mixture of Up and confirmed Down checks |
| Red | All enabled checks are confirmed Down |
| Gray | Disabled monitoring, no enabled checks, missing results, or stale results |

There are no mandatory public targets. First launch creates one empty LOCAL group in memory. Nothing is probed until checks are added and saved.

## Build

Install Visual Studio with **Desktop development with C++**, the MSVC x64 tools and a Windows 10/11 SDK. MFC is not required. Install CMake and Python 3. The checked-in preset targets Visual Studio 2026; use a recent CMake version that supports its generator.

From PowerShell in the repository root:

```powershell
cmake --preset windows-x64
cmake --build --preset release
ctest --preset release --output-on-failure
```

The plugin is `build/Release/NetWatch.dll`. Tests are built with `/W4 /WX`; third-party interface headers use the compiler's external-header treatment. The DLL uses the dynamic MSVC runtime, so the matching Microsoft Visual C++ x64 Redistributable must be installed on the target system.

For Visual Studio 2022, configure a separate build tree instead of using the 2026 preset:

```powershell
cmake -S . -B build-vs2022 -G "Visual Studio 17 2022" -A x64
cmake --build build-vs2022 --config Release
ctest --test-dir build-vs2022 -C Release --output-on-failure
```

The VS 2022 commands are provided for portability; the recorded local verification used VS 2026. Keep custom build trees out of Git.

## Install and configure

1. Exit the intended TrafficMonitor instance.
2. Copy `NetWatch.dll` to its `plugins` directory. Keep the bundled MIT license and upstream notice/license with redistributed packages.
3. Start TrafficMonitor 1.86 x64. Open **More Functions > Plugin Management**, select **NetWatch**, then open its options.
4. Add or select a group. Set its full name, short label, monitoring checkbox and taskbar checkbox independently. Use Up/Down to change group order.
5. Add checks in the same window. ICMP/TCP use a hostname, unbracketed IPv4/IPv6 address (scope IDs are allowed for IPv6); TCP also needs a port. HTTP uses a complete `http://` or `https://` URL in Host / URL.
6. Set the interval in seconds (default 30) and timeout in milliseconds (default 2000). **Test now (draft)** probes the edited check without saving it or changing monitoring history. A successful manual probe does not immediately make the taskbar circle green.
7. Save. After display changes, restart TrafficMonitor and select the new plugin items in its taskbar display settings.

Edits are captured when switching selection, testing or saving. Cancel discards the draft. Delete affects the draft until Save. The native dialog follows TrafficMonitor's language: Russian and English are supported, with English as the fallback for other languages. See [localization](docs/localization.md).

Click **Help** or press **F1** in settings for an offline guide covering setup, check types, indicator colors, timing, display/restart rules, manual testing and common errors. The guide uses the same language as settings, can be scrolled and copied, and opens without validating or saving unfinished edits.

`MaxVisibleGroups` accepts `All` or 0-4096. Zero exports no indicators. Otherwise the first N groups with Show in taskbar selected are exported in configuration order. Hidden groups continue to run if monitoring is enabled. Each group and check has a persistent UUID independent of its name and position.

**TrafficMonitor 1.86 enumerates plugin items only at startup.** Changing the effective visible item list, its order, names or labels requires a restart. Visibility and MaxVisibleGroups changes only require a restart when they affect that list. Save shows a restart notice only if the desired list differs from the items exported at load; reopening settings preserves that reminder until restart or until the original layout is restored. Existing item objects and their text remain valid until unload. Removed items turn gray until restart. Monitoring edits apply on Save and discard results from the previous configuration generation. TrafficMonitor's own item selection and ordering can further affect final presentation.

## Probes and status rules

- ICMP uses the Windows IPv4/IPv6 echo APIs. Only an actual successful echo reply provides ICMP RTT.
- TCP resolves IPv4/IPv6 addresses and attempts nonblocking connections within one overall deadline. TCP connect time excludes DNS. Refusal, timeout, unreachable and other connection errors are distinguished.
- HTTP performs a direct GET and stops after response headers. The measurement includes name resolution, connection establishment, TLS if applicable, and response headers. It is not ICMP RTT or TCP connect time. Only 2xx is successful. Redirects are disabled, so 3xx is a visible HTTP status failure and cannot downgrade HTTPS to HTTP. No response body is downloaded. Windows certificate validation remains enabled. Proxy configuration and credentials are not supported in this version.
- Failures without a completed measurement display `n/a`, not a fabricated zero latency. Error codes and HTTP status are shown separately.
- Three consecutive failures confirm Down. One success restores Up immediately. Before the first success, one or two failures remain Unknown. After success, transient failures preserve Up only while the last success is fresh.
- Results expire after `max(3 * interval, interval + timeout)`. A group with an unknown active check is gray. Disabled checks are excluded.

The scheduler allows at most **four concurrent probe workers and four starts per second**, including manual tests. There is no worker per group and no permanent scheduler thread. TrafficMonitor's DataRequired callback dispatches due work; a check's interval is measured from completion to its next eligibility. With many checks, scheduling may be delayed and results can expire. Increase intervals or reduce enabled checks rather than interpreting an overloaded gray group as healthy.

The tooltip is shared across the plugin because API v7 has no per-item tooltip. It includes group/check names, effective state, last raw error, typed measurement and local last-update time. Very large tooltips are truncated; individual checks can be tested in settings.

## Configuration and troubleshooting

The versioned UTF-8 configuration file is `NetWatch.conf` in the directory supplied by TrafficMonitor's `EI_CONFIG_DIR`, normally the host's configuration-directory `plugins` subdirectory. In portable mode this is beside the plugin DLL; in non-portable mode it may be under AppData. Saves write a temporary file in the same directory and atomically replace the destination. Failed parsing leaves the original file untouched and displays an error in settings and the tooltip. Back up a malformed file before deliberately saving a replacement.

- **Plugin missing:** confirm the host and DLL are both x64, check Plugin Management's load error and install the required MSVC runtime.
- **No indicators:** check MaxVisibleGroups, Show in taskbar, restart after layout changes, then enable the items in TrafficMonitor's taskbar options.
- **Gray circle:** add/enable checks, enable group monitoring, wait for initial results, or reduce the enabled workload if results are stale.
- **TCP timeout instead of refusal:** on some Windows systems a closed loopback port takes about two seconds to report refusal. A shorter timeout correctly reports Timeout.
- **TLS error:** inspect the URL, hostname, system clock and Windows trust configuration. NetWatch has no certificate-validation bypass.
- **Build cannot replace DLL:** exit only the disposable test host that loaded that build output before rebuilding. Do not terminate an installed host as a build step.

## Tests and development

CTest covers model/config behavior, the real hidden settings controller, the exported DLL ABI, GDI calls, concurrent getters, local network probes, upstream-file hashes and actual DLL unloading during an HTTP request confirmed by the local server. Network tests generate temporary self-signed certificates with OpenSSL. Git for Windows' bundled OpenSSL is detected automatically; alternatively place OpenSSL on PATH. IPv6 loopback must be enabled. ICMP failure uses the explicit documentation address `192.0.2.1`; all successful network tests use loopback.

To inspect settings without installing the plugin, run `build/Release/netwatch_host.exe` without arguments. Its preview configuration is isolated under `build/Release/preview-config`.

For an ABI host built against an independently downloaded, pinned official header:

```powershell
cmake --preset windows-x64 -DNETWATCH_REFERENCE_HEADER=D:/path/to/official/PluginInterface.h
cmake --build --preset release
ctest --preset release --output-on-failure
```

Verify that external file's SHA-256 against [the provenance record](vendor/README.md). The ordinary offline build uses the vendored header plus its pinned byte-hash test.

Prepare a disposable portable integration host:

```powershell
python tests/prepare_portable.py --dll build/Release/NetWatch.dll --output .work/integration
```

If another TrafficMonitor instance must keep running, add `--isolate-instance`. This changes only the copy's fixed-length instance-mutex string, records original/test hashes, and leaves installed files and running processes untouched. This modified copy is a test artifact, not a release distribution. The fixture exports UP, MIX, DOWN and NEW groups using only loopback and a deliberately invalid hostname. See [verification](docs/verification.md) for actual results and limits.

For the additional source-built V1.86 integration host, render capture and GUI-exit checks, see [integration testing](docs/integration.md). That optional host needs MFC; the plugin itself does not. The verified 150% GDI result is shown below.

![TrafficMonitor V1.86 taskbar GDI render at 150%](docs/images/Taskbar-v186-gdi-150.png)

Source layout: `model/config` hold policy and persistence; `probes` owns network APIs; `engine` schedules finite workers; `plugin` adapts API v7 and draws indicators; `settings/resources` implement the native dialog. Read [architecture and lifecycle](docs/architecture.md) before changing worker ownership, returned pointers, or item enumeration.

## Licensing

The proposed license for original NetWatch code is MIT; see [LICENSE](LICENSE). The unmodified TrafficMonitor interface retains its own [Anti-996 License](vendor/TrafficMonitor.LICENSE) and original attribution. The combined distribution must retain and comply with both notices; it is not MIT-only. See [third-party notices](THIRD_PARTY_NOTICES.md). No Ping Plugin or IPRegionMon implementation was copied. Nothing is pushed or published by the build.
