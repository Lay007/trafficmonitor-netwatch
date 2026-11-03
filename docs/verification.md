# Verification record

Updated: 2026-11-03. This work did not modify installed TrafficMonitor or IPRegionMon files. The original 2026-10-29 integration observations below describe an earlier DLL. Repository publication and installed-host deployment are separate from the local follow-ups recorded here.

## Build and automated tests

Windows x64, MSVC 19.51, Windows SDK 10.0.26100.0, Visual Studio 2026, CMake 4.4.4 and Python 3.13.15. The Release DLL and test executables built with `/W4 /WX`. The ABI host was compiled against a separately downloaded official V1.86 header, not the plugin's include path. Both copies matched the pinned upstream hash.

Executed:

```powershell
cmake --preset windows-x64 -DNETWATCH_REFERENCE_HEADER=D:/Programs/trafficmonitor-netwatch/.work/PluginInterface.reference.h
cmake --build --preset release
ctest --preset release --output-on-failure
```

Latest full-suite run: **9/9 passed**, 29.93 seconds, on 2026-11-03 after adding live check status in settings. Logs remain in the ignored `build/Testing/Temporary/LastTest.log`. The preceding tooltip-overflow fix passed 9/9 (27.23 seconds), Chinese-language run passed 8/8 (26.40 seconds), OpenSSH/default-groups passed 8/8 (25.85 seconds), and the earlier help follow-up passed 7/7 (25.68 seconds).

### Prerelease package preparation (2026-11-03)

Prepared `v0.1.0-beta.1` as a feedback prerelease, retaining numeric DLL/catalog version `0.1.0`. Packaging changes do not alter plugin source or DLL bytes from the live-table build below. The allowlisted ZIP includes the DLL, installation instructions, release notes, both license texts, notices, selected documentation/evidence and a file-hash manifest. It excludes TrafficMonitor executables, runtime installers, private notes and user configuration. The separate `SHA256SUMS.txt` identifies the exact ZIP; the DLL hash remains the Current Release DLL hash recorded below.

The initial packaged DLL was extracted and passed the existing ABI host (API v7/export/GDI/concurrent reads/active unload/100 reloads) and all-four-language tooltip-budget regression. Every manifest file hash and ZIP CRC was checked against the packaged bytes. Authenticode inspection returned `NotSigned`; SHA-256 is not a signing certificate. Final candidate hashes are retained with the ignored output artifacts, avoiding an archive-hash self-reference inside this bundled document.

English/Chinese descriptions and the numeric XML entry were prepared as a three-file patch against TrafficMonitorPlugins commit `eb7fc9e56f93fb69c99b185c6b8c395153d78bc6`. XML parsing found exactly one `NetWatch.dll` entry with version `0.1.0`; `git apply --cached --check` against the unchanged upstream index succeeded. The upstream [submission instructions](https://github.com/zhongyang219/TrafficMonitorPlugins/blob/main/README.md#插件的提交) were checked through the GitHub connector. The patch links the explicit prerelease tag, not `/releases/latest`.

At preparation time the public repository had no release/tag for this candidate. No release, tag, public asset, upstream PR or email was published. Existing desktop/DPI/D2D/soak limitations remain open and are stated in the package and catalog text. See [publication order](releases/README.md) for the separate authorization and post-publication checks.

### Live check status in settings (2026-11-03)

Replaced the check-name list with a five-column report ListView: Name, State, Time in milliseconds with ICMP/TCP/HTTP prefix, Last result and Checked. The UI polls copied engine snapshots every 500 ms without dispatching network work. Disabled, unknown, running, stale and unsaved rows are distinguished. Drafts are compared by UUID and saved definition, including the current unfinished editor fields; results from another destination or configuration generation are not shown as current draft results. Manual test output remains separate. The list area is taller and the scrollable manual-result area shorter; the dialog's outer dimensions are unchanged.

The real hidden Russian dialog/controller test drives actual loopback ICMP and invalid-name DNS monitoring until success and three-failure Down. It verifies typed measurements, raw DNS codes, failure count and local timestamp; disabled and untested rows; unsaved/new destinations without inherited results; selection recovery after an invalid edit; preserved unfinished fields during timer updates; reverted edits; independent manual history; stale-result display after stopping dispatch; live generation resets; changed saved definitions with the same UUID; and worker drain after closure. The network test also observes an in-flight HTTP snapshot and removal of its group on apply. Existing model/aggregation tests pass with the shared freshness predicate.

English and both Chinese native resources are instantiated by the existing controller suite; monitoring headers, Chinese Unsaved states and the new help paragraphs are asserted. Resource strings now use their full resource length rather than a 1024-character buffer. Intermediate checks caught the omitted Russian help paragraph and shell-corrupted translation text; only the final corrected UTF-8 resources and passing run are counted. `git diff --check` passed.

Desktop inspection could not run: the computer-use helper returned `failed to connect native pipe` / Windows error 2 on the initial call, retry and retry after resetting its runtime. The prepared synthetic preview under ignored `.work/live-status-ui-*/` was not launched. No visual/DPI/theme certification of the new table is claimed. The standalone DLL preview host now dispatches monitoring on a background thread so a later manual inspection can observe live saved results. Installed TrafficMonitor and IPRegionMon were not modified.

Current Release DLL SHA-256: `1f862ce53be680e052bec00663e65cc762da97881d3194a6c6930af7be9f5c91`.

### Repeated invalid-argument dialogs after SSH import (2026-11-03)

Reproduced in a disposable source-built V1.86 Lite host with the production DLL: native file selection, import nine synthetic SSH hosts (`127.0.0.1`, TCP ports 20000-20008), Save, close settings, then continue normal host monitoring. The host repeatedly displayed `Недопустимый аргумент.`. A main-thread exception stack reached `CTrafficMonitorDlg::OnMonitorInfoUpdated`, which updates the combined tooltip. The installed MFC source (`tooltip.cpp`, `CToolTipCtrl::UpdateTipText`) rejects text over 1024 characters with `ENSURE_ARG`. NetWatch previously allowed over 12000 characters. Cancelling file selection with an empty configuration did not reproduce the error; opening the picker alone was insufficient.

The exported NetWatch section now has a 512 UTF-16-code-unit budget, including load-error text and a localized truncation notice. Whole-line truncation and surrogate-pair preservation keep returned text valid. The unused half is shared with TrafficMonitor and other plugins; API v7 cannot reserve or inspect their combined budget.

The new `tooltip_budget` test loads the actual DLL with nine long check names and Unicode supplementary characters in all four languages. It failed before the fix (`Plugin tooltip exceeds its share of the host's 1024-character limit`) and passed afterward. Existing tests still cover short localized tooltips and pointer lifetime across readers.

The original native-import scenario was rerun with only the plugin DLL replaced in the same GDI fixture. Before the fix the driver stopped at the third identical host error (exit code 19). After the fix all nine checks persisted, settings returned, and the host ran for 45 seconds after closure without an error dialog, reaching the completion marker (exit code 0). The diagnostic driver uses `ExitProcess` at completion; these codes do not verify graceful shutdown. An import-address shim preselects the synthetic file before calling the real `GetOpenFileNameW`; file parsing, preview, Save, probe scheduling and host tooltip code remain real. Earlier attempts that cancelled the chooser or posted Save before preview acceptance are not counted as successful import reproductions.

Ignored diagnostic artifacts: `.work/repro-import-close-8321958511494c83a9ff6d3ed73593e4/TrafficMonitor/`, including `before-fix-native-three-errors.log` and `NetWatch-repro.log`. The instrumented test-host SHA-256 is `580d5fff0e9b7fe6b669f9130004d4bf4d0e9e548522c62fe46c7714b61e3538`. This is a source-built, instrumented GDI host, not certification of the installed/pristine binary or D2D. The installed TrafficMonitor and other plugins were not changed.

Tooltip-fix Release DLL SHA-256, before the live settings table: `d6e1ee7e873b1d92a3781730a2461145352a8d38124d71813d6fee613dcff277`.

### Simplified and Traditional Chinese (2026-11-02)

Added complete Chinese settings/help/SSH-preview resources plus translated status, validation, measurement, skip-reason, description and load/save explanation text. Host LANGID selection distinguishes Mainland/Singapore Simplified Chinese from Taiwan/Hong Kong/Macau Traditional Chinese, including the neutral Hant LANGID. English remains the fallback for other unsupported languages. User names, SSH aliases, IDs and persistent file tags remain unchanged.

Tests check LANGID precedence and regional mappings, nonempty Chinese resource strings across the catalog, both Chinese settings/help/SSH-preview resources, selected import, localized restart notices and persistence of the internal All token. The actual DLL is loaded with all four host-language implementations, and new Chinese group names are checked separately from preservation of a saved configuration. Compilation initially caught a shadowed local name in the expanded test; it was renamed, and only results following a successful build are counted.

Visual inspection used the actual Release DLL in the standalone host with explicit test-only `--ui-zh-cn`/`--ui-zh-tw` language flags and disposable `.work/` directories. Both Chinese settings windows displayed readable glyphs and accessible controls without obvious clipping on the current desktop. Simplified Chinese help opened by button, scrolled to SSH/default-group sections and closed; the settings window then closed through Cancel. Traditional Chinese settings also closed through Cancel. Both preview processes returned zero. No system/installed-host language or configuration was changed. This does not certify every DPI, theme, fallback font or native-speaker wording choice.

Chinese-language Release DLL SHA-256, before the tooltip fix: `28f52ed53783fd9969f1f8b5c379816ce3a8908958ce12c7423abdc53373d137`.

### OpenSSH import and default groups (2026-11-02)

Added bounded local OpenSSH parsing and a Russian/English import preview. The operation creates selected TCP checks in a draft, skips unsupported routes and duplicates, and requires normal Save to apply. First launch uses three empty localized groups; existing saved configurations are not supplemented automatically.

New `ssh_import` coverage: BOM/CRLF, quoting/comments, `=` syntax, aliases/patterns/negation, first-value precedence, Include ordering and parent-state restoration, inactive conditional Includes, IPv6/percent tokens, default/invalid ports, skipped proxy/canonicalization/dynamic values, rejected Match/cycles/malformed inputs, repeat imports preserving IDs, configuration round trips, an old single-group file remaining unchanged, and atomic rejection when the check limit is full. Existing tests that specifically exercise a single group now explicitly construct a one-group fixture; separate tests assert the new three-group production default.

The real settings-controller test exercises chooser cancellation, preview cancellation, destination defaults, selecting a subset, refusing a user-checked skipped row, repeated imports, additive default groups, no live/file changes before Save, and persistence after Save. File chooser/modal invocation is substituted with controlled test fixtures; the actual native preview resources, list view and controllers are used. The ABI host separately verifies three default items in Russian, no automatic file write and preservation of a saved one-group layout.

An independent `ssh -G -F` experiment on synthetic fixtures confirmed case-sensitive Host matching and first-value behavior in Windows OpenSSH. Its Include comparison was blocked by OpenSSH's `Bad owner or permissions` check on the disposable include file; no ACLs were changed and that run is not counted as a successful comparison. Reviewing upstream readconf.c revealed that Include restores the parent's Host state between files; the parser and a regression test explicitly enforce this. The plugin itself never invokes ssh or executes configuration commands.

The computer-use helper was available in this session. Visual inspection of the standalone DLL host confirmed the three Russian default groups, both new settings buttons, and the import preview's destination group, checkboxes and resolved alias/address/port columns. No obvious clipping was observed at that desktop configuration; a full DPI/theme matrix was not tested. A preview of an existing user-selected SSH file was visible, but its addresses and screenshots are not retained in public evidence. The preview process subsequently exited. These observations concern the standalone test host, not a pristine TrafficMonitor GUI certification; automated tests remain the evidence for controlled cancellation/Save scenarios.

SSH/default-groups DLL SHA-256, before Chinese resources: `cf0c598117e67e7580dd9991325760e8d172d4e0c9a9cd69a2a9ad097cd5f712`.

### Offline help follow-up (2026-10-30)

Both settings resources now expose a Help button. The button and `WM_HELP` (the F1 route) open a localized, read-only, scrollable help dialog using embedded resources. The seven sections cover setup, probe types, colors/hysteresis, timing/staleness/capacity, visibility/restart rules, manual testing/saving and error interpretation. No network request or configuration write is part of opening help.

The settings test substitutes modal invocation with a hidden modeless instance of the real help resource/controller. It verifies English/Russian captions, the presence of the final help section, read-only/scrollbar styles, the F1 message route, and opening help with an unfinished numeric field without capturing edits or launching a probe. Actual desktop keyboard/scroll behavior and visual layout of the new help window were not inspected. The user-provided screenshot of the preceding settings build confirms Russian settings text is visible, but does not verify this help addition.

The first build rejected `IDC_HELP` because Windows already defines that cursor identifier. The control was renamed `IDC_OPEN_HELP`. A test run accidentally launched after that failed build used the preceding executables and is not counted as evidence for help. The full-suite result above was obtained after a successful build.

Help follow-up DLL SHA-256, before SSH import/default groups: `1d8829a34e6364faebdf191d2b877f76eb58399df7def731c55020b1f4563622`.

### Save notice and Russian UI follow-up (2026-10-30)

Before the fix, the regression test exercised the actual settings controller: adding a check to the existing group and saving persisted the check but incorrectly displayed a restart notice. The test failed with `Check-only save incorrectly requests a host restart`.

The controller now compares the effective visible IDs, names, labels and order with the immutable list exported at plugin initialization. The same test passes without a notice. A changed label produces one localized notice; reopening settings before restart still produces it; using a freshly exported layout removes it for later check-only saves. Both paths verify the saved file and live configuration. Model tests also cover names, ordering, visible/hidden additions, removal, zero/equivalent limits and monitoring-only changes.

Russian dialog resources, notification strings, validation, probe statuses, manual results and tooltips were added. Tests instantiate both real dialog resources, verify Russian Unicode captions/buttons and `Все`, persist the unchanged `All` file token, and obtain a localized loopback ICMP result. Language-selection tests cover host-over-Windows precedence, absent host language and unsupported-language fallback. The ABI test loads the actual DLL with Russian and English host implementations and verifies localized descriptions/tooltips while preserving configured names.

An intermediate test caught question marks in the generated Russian resource due to the shell pipeline's encoding. The resource was rewritten directly as UTF-8 with an explicit resource-compiler code page; the exact Russian caption assertion then passed.

Desktop inspection of the updated Russian dialog was attempted but unavailable: the computer-use helper returned `failed to connect native pipe` / Windows error 2 on the initial call, retry and retry after runtime reset. Automated resource/controller tests passed; visual clipping and desktop Save behavior of this updated DLL are not claimed as verified. The earlier English source-host GUI evidence below remains historical.

Save-notice/localization DLL SHA-256, before the help addition: `eafe5a3685671d94b01dd4aaec076ac1bddcc2036445630bf9c95b60b9c42ba5`.

| Test | Actual coverage |
| --- | --- |
| settings | Hidden real dialog resource/controller: editing, stable IDs, visibility/monitoring, limits, validation, live monitoring table with success/failure/stale/disabled/unsaved rows, preserved selection/edits, generation reset, independent manual result polling and close during pending ICMP |
| core | Group/check IDs, order and visibility limits, UTF-8/Unicode and Windows BOM/CRLF config round trips, atomic rejection of invalid saves, malformed config rejection, three-failure debounce, one-success recovery, stale-green expiry, mixed/disabled/empty/unknown aggregation |
| ssh_import | Bounded OpenSSH reading, supported matching/precedence/Include behavior, unsupported syntax/routes, duplicate-safe TCP conversion, localized empty defaults and preservation of existing configurations |
| plugin_host | Actual LoadLibrary/GetProcAddress export, singleton, API v7 virtual slots, item boundaries/IDs, GDI calls, concurrent getter calls, returned-pointer stability, release during a probe, eventual unload, 100 reloads and handle/GDI counts |
| tooltip_budget | Actual DLL output capped at 512 UTF-16 code units for a large configuration, localized truncation notice in all four languages, valid surrogate pairs |
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

The independent settings preview was visually inspected through the computer-use skill: controls fit, adding a check populated its fields, and a manual loopback ICMP result appeared. Saving through that early preview was not recorded as successful because of geometry errors. A later host integration test below exercised GUI saving successfully.

The release binary's floating/taskbar windows were not exposed as targetable windows by the computer-use helper. Upstream gives both windows WS_EX_TOOLWINDOW. A duplicate-instance warning was observable, but its buttons were not used because upstream resolves their target by a global window class. The alternative app-control runtime also failed to initialize with `failed to write kernel assets`.

### Additional source-built integration host

To inspect the real host path without manipulating an installed process, `tests/prepare_source_host.py` builds an additional disposable Lite host from the exact V1.86 source commit. It requires VS 2026 with MFC, which is an extra test-host dependency, not a NetWatch dependency. Its changes are limited to unique instance-mutex/window-class names, a regular main-window style for computer use, explicit UTF-8 compilation and an opt-in timer hook. The hook calls the live taskbar object's **unmodified ShowInfo renderer** into a bitmap and reads the text already stored in its tooltip control. It does not change plugin loading, layout calculation, network probes, item drawing or shutdown handlers. The custom window class is changed in both registration and the dialog resource.

The observed host lives in `.work/source-integration/fixture/TrafficMonitor`. Its recorded SHA-256 is `50ec96adbbf82faf5c5290a758ad8035299edf34ab53d6e5a39d74f4b50dc62f`. It loaded the exact production DLL, SHA-256 `c06fa56cdb84fef59de0e2c4054ef371614986f177d5006ffa3152ba58dcdc63`. This is a modified, source-built integration host, not certification of the pristine release binary.

Confirmed results:

- The live taskbar window reported `visible=1` and successful insertion into the Windows taskbar. Its client size was **177 x 48 pixels at DPI 144 (150%)**. Its GDI renderer produced the four expected labels and colored circles shown below. This is a render capture from the live host, not a desktop screenshot.
- The host's tooltip control contained all four groups, typed ICMP measurements, raw DNS errors, failure counters and timestamps. The retained [NetWatch-only tooltip excerpt](evidence/tooltip-v186.txt) excludes unrelated system metrics. The popup's actual hover/placement behavior was not visually exercised.
- In the actual host's Plugin Management dialog, NetWatch 0.1.0 reported **Load succeed**. Opening Options showed the native NetWatch settings dialog. Unchecking Monitor this group and clicking Save changed the persisted group-enabled field from 1 to 0 while preserving group/check UUIDs. The host displayed the save/restart notice and returned to Plugin Management.
- Exit was selected through the actual host's context menu, with no forced termination. A control run with NetWatch disabled and absent from the loaded module list returned code **2**. The NetWatch run during a held loopback HTTP response returned the same code **2**. At process exit the request was **21.152 seconds old**, below its **30-second timeout**, and the server had completed zero responses. A process handle opened before the click confirmed termination. This tests normal process exit, not graceful completion of each outstanding probe after process termination.

![TrafficMonitor V1.86 GDI renderer at 150% DPI](images/Taskbar-v186-gdi-150.png)

The initial HTTP exit attempt happened after the request budget had elapsed and was not counted as an active-request test. The successful repeat recorded the process-exit timestamp and checked it against server receipt time. The observer initially assumed an exit code of zero; that assumption was rejected by the plugin-disabled control. The installed MFC source and TrafficMonitor's ExitInstance delegate confirm that modal/message return codes can be propagated. All observed codes are retained verbatim in the ignored evidence files.

Reproduction and raw artifacts are described in [integration testing](integration.md). The preparation script was also run from start to finish in a fresh `.work/source-integration-repro` directory and built successfully. The release DLL was not changed by this follow-up.

## Failures encountered and corrected

1. A 500 ms TCP test observed Timeout, not refusal. An independent Python socket returned WSAECONNREFUSED after about 2.03 seconds on this machine. The refusal-specific fixture now waits up to 3000 ms; production behavior still respects its configured timeout.
2. A WinHTTP subscription included intermediate sending events. The state waiter rejected one as an internal error. Subscription now requests SENDREQUEST_COMPLETE, HEADERS_AVAILABLE and REQUEST_ERROR only.
3. An initial TLS fixture used localhost, which could try an unavailable IPv6 listener before the IPv4 TLS server. The fixture now explicitly uses 127.0.0.1. Certificate checks stay enabled.
4. The first HTTP-unload test released the host reference after a fixed sample and sometimes hit an idle scheduling gap. It now waits for a server-confirmed active request. A backslash in the initial event URL was normalized by WinHTTP; the test event uses a URL-safe session-local name.
5. Rebuilding while the preview held NetWatch.dll open failed with LNK1104. The disposable preview was closed before rebuilding. A failed build is not treated as validation of a new binary.

## Remaining limits

- A successful trusted HTTPS exchange was not exercised against a controlled trusted-CA fixture; invalid TLS rejection was exercised. No Windows trust settings were changed.
- DNS-provider cancellation and WinHTTP callback draining must finish before stack state or DLL code can be released. The I/O budget is bounded, but abnormal OS/provider stalls can defer cleanup beyond that budget. Forced process termination is not graceful shutdown.
- Ordinary FreeLibrary during active ICMP/HTTP and normal GUI exit of the source-built host during active HTTP were tested. Pristine-binary GUI exit and long-duration CPU/memory behavior were not.
- x86/ARM64, alternate TrafficMonitor releases, unusual DNS providers, D2D, per-monitor DPI transitions and a complete dark/light theme matrix were not verified. Russian and both Chinese settings were observed in the standalone host; the full UI matrix and native-speaker Chinese review remain open. The earlier source-host GDI layout used 150% DPI.
- The scheduler intentionally sacrifices timeliness for bounded work. Large configurations can become gray because results expire. There is no promise of exact intervals under overload.
