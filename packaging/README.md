# NetWatch v0.1.0-beta.1 for TrafficMonitor

NetWatch monitors user-defined groups of devices and services from the Windows taskbar using ICMP, TCP and HTTP/HTTPS checks. This is a prerelease for feedback.

- Target: Windows 10/11 x64 and TrafficMonitor 1.86 x64, plugin API v7.
- Runtime: the latest Microsoft Visual C++ v14 x64 Redistributable, at least as recent as the build tools. See [Microsoft's download page](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist).
- Languages: English, Russian, Simplified Chinese and Traditional Chinese, selected from TrafficMonitor's language.
- The version displayed in Plugin Management is `0.1.0`. The distribution tag is `v0.1.0-beta.1`; the GitHub release is marked as a prerelease.

## Install or update

1. Download the ZIP and `SHA256SUMS.txt` from [this release](https://github.com/Lay007/trafficmonitor-netwatch/releases/tag/v0.1.0-beta.1). In PowerShell, run `Get-FileHash .\NetWatch-v0.1.0-beta.1-windows-x64.zip -Algorithm SHA256` and compare with the checksum file.
2. Back up your existing `NetWatch.conf` if updating. Configuration belongs to your TrafficMonitor installation/profile; this archive does not include or replace it.
3. Exit the TrafficMonitor instance you intend to update. Extract the ZIP to a separate folder and copy **only `NetWatch.dll`** into its `plugins` directory, beside other plugin DLLs. Keep the license/notice files with the distribution if redistributing it.
4. Start TrafficMonitor, open **More Functions > Plugin Management**, select NetWatch and open its options. First launch has empty LOCAL, WEB and SSH groups, with no automatic public targets.
5. Add checks or use **Import SSH** to read your OpenSSH config. Imported hosts become TCP port checks, not SSH logins. Review the preview and Save.
6. After adding visible groups or changing labels/order/visibility, fully restart TrafficMonitor. Right-click its taskbar indicators, open **Display settings** and enable the new groups. The checkbox inside NetWatch only controls whether an item is offered to the host.

The settings table updates saved monitoring results every 500 ms. It shows state, typed probe time, last result/error and local check time. Unsaved edits have no monitoring result; **Test now (draft)** output is separate and does not update monitoring history. Use Help/F1 for the offline guide.

Green means all enabled checks are available; yellow means mixed availability; red means all are confirmed unavailable; gray means missing/stale results, no enabled checks, or disabled monitoring. Three consecutive failures confirm Down; one success restores Up. Four active workers and four starts per second are shared by monitoring and manual tests.

## Known limits and feedback

The new settings table has automated controller coverage but has not been visually checked across desktop/DPI/theme combinations. D2D, long-duration behavior, sleep/wake and a controlled trusted-HTTPS success case remain unverified. Other host versions and architectures are not certified. Large configurations can run late and show stale results. The host has a shared 1024-character tooltip limit; NetWatch contributes at most 512, and other plugins can still exhaust the remaining space.

SSH import supports a bounded subset of OpenSSH configuration. It does not authenticate, read keys or execute commands. ProxyJump/ProxyCommand and unsupported expansions are skipped. See [SSH import](docs/ssh-import.md), [language behavior](docs/localization.md), [verification](docs/verification.md) and [release notes](RELEASE_NOTES.md).

Report reproducible problems at [GitHub Issues](https://github.com/Lay007/trafficmonitor-netwatch/issues). Include TrafficMonitor/NetWatch versions, Windows version, DPI, exact error and reproduction steps. Redact private addresses, host aliases, usernames and credentials from screenshots/configuration before sharing.

Original NetWatch code uses the [MIT license](LICENSE). The vendored TrafficMonitor interface retains its [Anti-996 License](vendor/TrafficMonitor.LICENSE). Retain both and [third-party notices](THIRD_PARTY_NOTICES.md). TrafficMonitor, its modified test hosts and the Microsoft runtime are not bundled.

Source: [Lay007/trafficmonitor-netwatch](https://github.com/Lay007/trafficmonitor-netwatch). `manifest.json` records the source commit and hashes of the packaged files. `SHA256SUMS.txt` verifies the ZIP's integrity; it is not a code-signing certificate.
