# Verification record

Date: 2026-10-29. No installed TrafficMonitor or IPRegionMon files were changed. No remote repository was created; nothing was pushed or deployed.

## Build and automated tests

Windows x64, MSVC 19.51, Windows SDK 10.0.26100.0, Visual Studio 2026, CMake 4.4.4 and Python 3.13.15. The Release DLL and test executables built with `/W4 /WX`. The ABI host was compiled against a separately downloaded official V1.86 header, not the plugin's include path. Both copies matched the pinned upstream hash.

Executed:

```powershell
cmake --preset windows-x64 -DNETWATCH_REFERENCE_HEADER=D:/Programs/trafficmonitor-netwatch/.work/PluginInterface.reference.h
cmake --build --preset release
ctest --preset release --output-on-failure
```

Final recorded run: **7/7 passed**, 24.73 seconds. Logs remain in the ignored `build/Testing/Temporary/LastTest.log`.

| Test | Actual coverage |
| --- | --- |
| settings | Hidden real dialog resource/controller: add, rename, reorder, delete, stable IDs, checks, independent visibility/monitoring, zero display limit, numeric validation, manual draft result polling, destroy dialog during pending ICMP |
| core | Group/check IDs, order and visibility limits, UTF-8/Unicode and Windows BOM/CRLF config round trips, atomic rejection of invalid saves, malformed config rejection, three-failure debounce, one-success recovery, stale-green expiry, mixed/disabled/empty/unknown aggregation |
| plugin_host | Actual LoadLibrary/GetProcAddress export, singleton, API v7 virtual slots, item boundaries/IDs, GDI calls, concurrent getter calls, returned-pointer stability, release during a probe, eventual unload, 100 reloads and handle/GDI counts |
| render_and_layout | Actual DLL probes produce green/yellow/red/gray circles, checked by GDI bitmap pixels; item pointer and ID remain stable until unload; zero visible groups applies after reload |
| network | Open/refused IPv4 TCP, IPv6 TCP, IPv4/IPv6 ICMP success, invalid-name DNS failure, explicit TEST-NET ICMP timeout, HTTP 200/503/302, total HTTP timeout, untrusted TLS rejection, no TLS downgrade, 256 hidden groups with four-worker cap, config edits during blocked requests, obsolete-result rejection, eight concurrent readers, manual testing |
| provenance | Exact pinned upstream header and license bytes |
| http_unload | Actual DLL with 256 hidden HTTP checks; local server signals a named event to confirm an active request; host releases its LoadLibrary reference; DLL remains resident until workers cancel/drain and then unloads |

All successful network fixtures use loopback. The deliberately unsuccessful ICMP target is the explicit documentation address `192.0.2.1`. OpenSSL creates temporary self-signed certificates/keys; no private key or certificate is installed in Windows or committed.

## Resource observations

These are short observations, not long-term bounds or measurements of plugin-only overhead.

- Native HTTP stress host, 256 hidden checks, 8-second sample: peak working set **13,635,584 bytes** (13.0 MiB), handles **86 to peak 235**, total process threads **4 to peak 18**, CPU time **281.25 ms**. The host polls scheduling every 100 ms and includes resource-sampling overhead. Total threads include Windows/WinHTTP pools; plugin concurrency is separately limited to four workers. No resource thresholds were tuned to make the test pass.
- Disposable TrafficMonitor process with the four-group fixture, 10.019-second sample: working set **44,306,432 bytes**, private bytes **11,927,552**, **7 threads**, **466 handles**, CPU time **46.875 ms**. NetWatch.dll was present in its loaded module list. This includes TrafficMonitor itself; there was no matched plugin-disabled baseline, so it does not establish NetWatch's isolated cost.
- The ABI host checks no handle growth beyond a two-handle tolerance and no GDI-object growth across 100 reloads. This is not a multi-hour leak test.

## Portable integration and GUI

Official archive: `TrafficMonitor_V1.86_x64_Lite.zip`, SHA-256 `d9774a64268f952b5cb88a1eead399f863f7d4ab64e945f19c096fed89cc1145`.

The disposable fixture lives in `.work/integration/TrafficMonitor`. Its executable file version is 1.8.6.0. Because another TrafficMonitor process was already running, the test copy changed exactly one fixed-length instance-mutex string. Original EXE SHA-256: `6a907d36c3edbef14a4c10d7f6d9ae9585026fe9c361885c9e8fcf2254633c43`; isolated copy: `02e4c956426f656bb75cd1aec7098458c94c0b46e9441666727ed112390f93a6`. The script records these in `provenance.json`. Other executable bytes are unchanged. This is explicitly a modified integration host, not a pristine-binary certification.

Confirmed by process inspection: the portable host started, remained responsive, and loaded this project's NetWatch.dll from its own plugins directory. No installed executable, installed plugin or existing process was replaced or stopped.

The independent settings preview was visually inspected through the computer-use skill: controls fit, adding a check populated its fields, and a manual loopback ICMP result appeared. Later GUI input attempts failed with geometry errors, so saving through that preview was not recorded as successful. Save/load is covered by the native controller/config tests.

The portable host's floating/taskbar windows were not exposed as targetable windows by the available computer-use helper, even after retrying the visible launch and explicit window-position settings. A duplicate-instance warning was observable, but its buttons were not used because upstream resolves their target by a global window class. **Actual taskbar appearance, per-monitor DPI behavior, host tooltip presentation and normal GUI exit during active requests remain unverified.** Cross-DLL GDI drawing and safe ordinary FreeLibrary behavior are covered independently; those tests do not substitute for a completed GUI integration test.

## Failures encountered and corrected

1. A 500 ms TCP test observed Timeout, not refusal. An independent Python socket returned WSAECONNREFUSED after about 2.03 seconds on this machine. The refusal-specific fixture now waits up to 3000 ms; production behavior still respects its configured timeout.
2. A WinHTTP subscription included intermediate sending events. The state waiter rejected one as an internal error. Subscription now requests SENDREQUEST_COMPLETE, HEADERS_AVAILABLE and REQUEST_ERROR only.
3. An initial TLS fixture used localhost, which could try an unavailable IPv6 listener before the IPv4 TLS server. The fixture now explicitly uses 127.0.0.1. Certificate checks stay enabled.
4. The first HTTP-unload test released the host reference after a fixed sample and sometimes hit an idle scheduling gap. It now waits for a server-confirmed active request. A backslash in the initial event URL was normalized by WinHTTP; the test event uses a URL-safe session-local name.
5. Rebuilding while the preview held NetWatch.dll open failed with LNK1104. The disposable preview was closed before rebuilding. A failed build is not treated as validation of a new binary.

## Remaining limits

- A successful trusted HTTPS exchange was not exercised against a controlled trusted-CA fixture; invalid TLS rejection was exercised. No Windows trust settings were changed.
- DNS-provider cancellation and WinHTTP callback draining must finish before stack state or DLL code can be released. The I/O budget is bounded, but abnormal OS/provider stalls can defer cleanup beyond that budget. Forced process termination is not graceful shutdown.
- Ordinary FreeLibrary during active ICMP/HTTP was tested; normal TrafficMonitor GUI exit and long-duration CPU/memory behavior were not.
- x86/ARM64, alternate TrafficMonitor releases, translated UI, unusual DNS providers and high-DPI/dark/light theme matrices were not verified.
- The scheduler intentionally sacrifices timeliness for bounded work. Large configurations can become gray because results expire. There is no promise of exact intervals under overload.
