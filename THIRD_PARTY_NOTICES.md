# Third-party notices

`vendor/PluginInterface.h` is from TrafficMonitor by Zhong Yang, copyright 2021. Its original attribution is preserved. The associated Anti-996 License and repository notice are retained verbatim in `vendor/TrafficMonitor.LICENSE`. See `vendor/README.md` for the immutable commit and byte hashes.

The root MIT license covers original NetWatch files, not the vendored TrafficMonitor interface. Redistributed binaries are built using that interface; retain both license texts and this notice, and comply with the upstream conditions.

Windows SDK declarations and system libraries (Winsock, IP Helper/ICMP, WinHTTP, COM, GDI, User32 and common controls) are used through their installed development/runtime environments. No third-party runtime libraries are bundled. The dynamic Microsoft C++ runtime is a system prerequisite, not included in this repository.

TrafficMonitor Ping Plugin and the neighboring IPRegionMon project were inspected as references. No source, modified interface headers, icons or other assets were imported from them.

Downloaded TrafficMonitor binaries under ignored `.work/` are integration fixtures only, governed by their upstream license. They are not part of NetWatch's source distribution. `--isolate-instance` creates a modified disposable host with an explicitly recorded provenance, not a modified plugin ABI.
