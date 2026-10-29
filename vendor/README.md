# Pinned TrafficMonitor interface

Upstream repository: https://github.com/zhongyang219/TrafficMonitor

Tag: V1.86. Commit: `02a817a069bac6bf4d263b5209d9c1b07fe2f950`.

Both files retain upstream bytes, including LF line endings. `.gitattributes` disables text conversion for them.

| Local file | Upstream path | SHA-256 |
| --- | --- | --- |
| PluginInterface.h | [include/PluginInterface.h](https://github.com/zhongyang219/TrafficMonitor/blob/02a817a069bac6bf4d263b5209d9c1b07fe2f950/include/PluginInterface.h) | `1760d2cdf15da70a567bff367e79e470a8ee6ab33084c19a0e61a50153958f9b` |
| TrafficMonitor.LICENSE | [LICENSE](https://github.com/zhongyang219/TrafficMonitor/blob/02a817a069bac6bf4d263b5209d9c1b07fe2f950/LICENSE) | `7523fa7601abdf090a91ca365d3a768e5418292dc151038d0629e67240a33fe6` |

The header reports API v7. It has no virtual destructors, IPluginDrawer, DrawItemEx or IsDoubleLineExclusive. Those newer API v8 additions must not be assumed to exist in the 1.86 ABI. Never reorder or add virtual methods in this copy. The host retains borrowed plugin/item pointers and does not delete them.

`tests/provenance.py` verifies both files offline. The ABI host can additionally compile against a separately downloaded official header through `NETWATCH_REFERENCE_HEADER`.
