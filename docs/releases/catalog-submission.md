# TrafficMonitorPlugins submission draft

Prepared for [the upstream submission instructions](https://github.com/zhongyang219/TrafficMonitorPlugins/blob/main/README.md#插件的提交), checked on 2026-11-03. The maintainer accepts a PR updating both download pages and `plugins_version.xml`, or an email with description and download URL.

Do not submit this draft until the prerelease tag and asset links below exist and the packaged download has been verified. The release is a feedback prerelease, and acceptance into the catalog is the maintainer's decision. Numeric host/catalog version `0.1.0` deliberately matches the DLL; `v0.1.0-beta.1` identifies the distribution. Do not use `/releases/latest` for this prerelease.

## Append to download/plugin_download_en.md

### NetWatch

Network availability groups for TrafficMonitor 1.86 x64 on Windows 10/11 x64. Monitor ICMP, TCP and HTTP/HTTPS checks using compact taskbar status circles, with live per-check results in settings. Import selected hosts from a local OpenSSH config as TCP port checks without SSH authentication or command execution. English, Russian, Simplified Chinese and Traditional Chinese are supported. No public targets are added automatically. This is a feedback prerelease; desktop/DPI/theme verification is incomplete.

* Version: 0.1.0 (v0.1.0-beta.1 prerelease)
* Author: [Lay007](https://github.com/Lay007)
* Homepage: [Lay007/trafficmonitor-netwatch](https://github.com/Lay007/trafficmonitor-netwatch)
* Download url (x64): [v0.1.0-beta.1](https://github.com/Lay007/trafficmonitor-netwatch/releases/tag/v0.1.0-beta.1)

## Append to download/plugin_download.md

### NetWatch

适用于 Windows 10/11 x64 和 TrafficMonitor 1.86 x64 的网络可用性分组插件。支持 ICMP、TCP 和 HTTP/HTTPS 检测，在任务栏以状态圆点显示分组可用性，并在设置中显示各检测项的实时结果。可从本地 OpenSSH 配置中选择主机并导入为 TCP 端口检测，不进行 SSH 身份验证或执行命令。支持英语、俄语、简体中文和繁体中文，不会自动添加公共检测目标。本版本为反馈测试用预发布版本，桌面布局、DPI 和主题验证尚未完成。

* 版本：0.1.0（v0.1.0-beta.1 预发布版）
* 作者：[Lay007](https://github.com/Lay007)
* 插件主页：[Lay007/trafficmonitor-netwatch](https://github.com/Lay007/trafficmonitor-netwatch)
* 下载地址（x64）：[v0.1.0-beta.1](https://github.com/Lay007/trafficmonitor-netwatch/releases/tag/v0.1.0-beta.1)

## Add under plugins in plugins_version.xml

```xml
    <plugin file_name="NetWatch.dll" version="0.1.0"/>
```

## Proposed PR title

Add NetWatch network availability plugin (x64 prerelease)

## Proposed PR body

NetWatch adds user-defined availability groups to TrafficMonitor's taskbar with ICMP, TCP and HTTP/HTTPS checks, live per-check results in settings and optional local OpenSSH endpoint import. It supports TrafficMonitor 1.86 x64 and English, Russian, Simplified Chinese and Traditional Chinese UI.

This change adds English and Chinese download descriptions and registers `NetWatch.dll` version `0.1.0`. The linked `v0.1.0-beta.1` package is explicitly a feedback prerelease. Its new settings table has automated controller tests but no completed desktop/DPI/theme inspection; detailed verification limits are in the release notes.

Source and download: https://github.com/Lay007/trafficmonitor-netwatch/releases/tag/v0.1.0-beta.1

No public targets, SSH keys, authentication or command execution are added. The distribution includes the NetWatch MIT license and upstream Anti-996 interface license. If prereleases do not belong in the catalog/update index, please defer the entry until a stable release rather than treating this build as stable.
