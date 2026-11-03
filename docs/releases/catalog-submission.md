# TrafficMonitorPlugins submission draft

Prepared for [the upstream submission instructions](https://github.com/zhongyang219/TrafficMonitorPlugins/blob/main/README.md#插件的提交), checked on 2026-11-03. The maintainer accepts a PR updating both download pages and `plugins_version.xml`, or an email with description and download URL.

Do not submit this draft until the prerelease tag and asset links below exist and the packaged download has been verified. The release is a feedback prerelease, and acceptance into the catalog is the maintainer's decision. Numeric host/catalog version `0.1.0` deliberately matches the DLL; `v0.1.0-beta.1` identifies the distribution. Do not use `/releases/latest` for this prerelease.

## Append to download/plugin_download_en.md

### NetWatch

Monitor groups of devices and services with ICMP, TCP and HTTP/HTTPS checks. Each group appears as a label and status circle on the taskbar; settings show the result and response time for each check. Hosts can be imported from a local OpenSSH config as TCP port checks. Supports TrafficMonitor 1.86 x64 on Windows 10/11 x64, with English, Russian, Simplified Chinese and Traditional Chinese UI. This is a beta; desktop layout, DPI and theme checks are still incomplete.

* Version: 0.1.0 (v0.1.0-beta.1 prerelease)
* Author: [Lay007](https://github.com/Lay007)
* Homepage: [Lay007/trafficmonitor-netwatch](https://github.com/Lay007/trafficmonitor-netwatch)
* Download url (x64): [v0.1.0-beta.1](https://github.com/Lay007/trafficmonitor-netwatch/releases/tag/v0.1.0-beta.1)

## Append to download/plugin_download.md

### NetWatch

按分组监测设备和服务，支持 ICMP、TCP 和 HTTP/HTTPS 检测。每个分组在任务栏显示名称和状态圆点，设置窗口中可查看各检测项的结果和响应时间。可从本地 OpenSSH 配置导入主机，生成 TCP 端口检测。适用于 Windows 10/11 x64 和 TrafficMonitor 1.86 x64，支持英语、俄语、简体中文和繁体中文界面。目前为测试版，桌面布局、DPI 和主题检查尚未完成。

* 版本：0.1.0（v0.1.0-beta.1 预发布版）
* 作者：[Lay007](https://github.com/Lay007)
* 插件主页：[Lay007/trafficmonitor-netwatch](https://github.com/Lay007/trafficmonitor-netwatch)
* 下载地址（x64）：[v0.1.0-beta.1](https://github.com/Lay007/trafficmonitor-netwatch/releases/tag/v0.1.0-beta.1)

## Add under plugins in plugins_version.xml

```xml
    <plugin file_name="NetWatch.dll" version="0.1.0"/>
```

## Proposed PR title

Add NetWatch network monitoring plugin

## Proposed PR body

Adds NetWatch to the English and Chinese plugin lists and registers `NetWatch.dll` version `0.1.0` in the update index.

The plugin groups ICMP, TCP and HTTP/HTTPS checks into taskbar indicators. Settings show each check's state, response time and last error. It can also import OpenSSH hosts as TCP port checks without logging in. The target is TrafficMonitor 1.86 x64 on Windows 10/11 x64; the interface supports English, Russian and both Chinese variants.

Source and download: https://github.com/Lay007/trafficmonitor-netwatch/releases/tag/v0.1.0-beta.1

This is the first beta. All nine automated tests pass, but the new settings table still needs desktop/DPI/theme checks. The release notes describe the remaining limits. The ZIP includes both the NetWatch MIT license and the upstream Anti-996 interface license.

If the catalog only accepts stable releases, I can resubmit after a stable build.
