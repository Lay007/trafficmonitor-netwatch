# Architecture and compatibility decision

Status: accepted for the first version, 2026-10-29.

## Upstream evidence

TrafficMonitor tag V1.86 resolves to commit `02a817a069bac6bf4d263b5209d9c1b07fe2f950`.
The header at that tag reports API **7**, not the newer master API 8.

- [PluginManager.cpp](https://github.com/zhongyang219/TrafficMonitor/blob/02a817a069bac6bf4d263b5209d9c1b07fe2f950/TrafficMonitor/PluginManager.cpp): LoadPlugins sends EI_CONFIG_DIR, calls OnInitialize, then enumerates GetItem until null exactly once. It stores raw item pointers. Its destructor calls FreeLibrary without a shutdown callback or deleting the plugin.
- [PluginInterface.h](https://github.com/zhongyang219/TrafficMonitor/blob/02a817a069bac6bf4d263b5209d9c1b07fe2f950/include/PluginInterface.h): no virtual destructor, no item-refresh callback, no per-item tooltip. Do not add virtual slots or claim API 8.
- [TrafficMonitor.cpp](https://github.com/zhongyang219/TrafficMonitor/blob/02a817a069bac6bf4d263b5209d9c1b07fe2f950/TrafficMonitor/TrafficMonitor.cpp): GetPlauginTooltipInfo concatenates plugin-wide tooltips.
- TaskBarDlg.cpp passes a GDI HDC to DrawItem, directly or through its GDI bridge. GetItemWidthEx measures with the selected host font.
- [Ping plugin](https://github.com/wwwroot/TrafficMonitorPingPlugin) uses IcmpSendEcho. Its simplified interface header is not used here. No implementation files were copied.
- The neighboring IPRegionMon project's ABI and concurrency documentation was inspected read-only. Its module-reference and callback-drain lessons inform this design; no source was copied.

## Display layout

Alternative A: export fixed slots and remap them to groups. This ties TrafficMonitor's persisted item settings to slots instead of group identities and introduces an artificial slot limit.

Chosen alternative B: export one immutable item per visible group at load. Item IDs are `netwatch.<group UUID>`. All pointers, names and labels live until unload. Adding/removing/reordering groups, changing labels, visibility or MaxVisibleGroups requires restarting TrafficMonitor. Monitoring changes apply immediately. Removed items remain gray until restart. The settings window explains this contract.

MaxVisibleGroups is either All or a nonnegative integer. Visibility is independent of monitoring. The first N selected groups in configuration order are exported. TrafficMonitor's own item selection and ordering still control final presentation.

## Components

- model: configuration, validation, stable UUIDs, freshness, failure hysteresis, aggregation.
- config: versioned UTF-8 file, strict parsing, same-directory atomic replacement.
- probes: Windows ICMP/ICMPv6, nonblocking Winsock TCP, async WinHTTP GET through headers. A shared deadline includes DNS and protocol work. Redirects are disabled; only 2xx succeeds. TLS uses Windows trust validation.
- engine: short locks around snapshots and scheduling. At most four finite workers and four starts per second, no permanent scheduler. DataRequired only dispatches due work. Manual tests share both limits. Results from old configuration generations are discarded.
- plugin: exact upstream ABI, immutable item list, GDI text/circle, plugin-wide tooltip.
- settings: one native modal dialog, editable local configuration copy, explicit save and manual probe. English resources can be translated through Windows resource tables.

## Lifetime

The host owns its LoadLibrary reference and must stop calling plugin methods before releasing it. The plugin owns its singleton and items; the host must not delete them. Each worker takes an additional GetModuleHandleEx reference before launching. C++ captures, probe handles and callback state are destroyed before FreeLibraryAndExitThread. No joins, locks, configuration writes or network operations run from DllMain or static destruction. Workers are finite; an ordinary FreeLibrary can defer actual unloading until outstanding probes drain. Process termination is not a graceful shutdown guarantee.

References: [FreeLibraryAndExitThread](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-freelibraryandexitthread), [DLL best practices](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices), [WinHTTP options](https://learn.microsoft.com/en-us/windows/win32/winhttp/option-flags).

## Status rules

Three consecutive failures confirm Down; one success restores Up. Before any success, the first two failures remain Unknown. A single failure after Up preserves Up only while the last success is fresh. Last success/result becomes stale after max(3 * interval, interval + timeout). Disabled and missing checks are excluded. No active checks or any unknown active check makes the group Unknown; otherwise all Up is green, all Down red, mixed yellow. Individual last errors remain visible during hysteresis.
