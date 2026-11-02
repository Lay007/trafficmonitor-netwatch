# Localization

NetWatch reads `ITrafficMonitor::GetLanguageId()` during `OnInitialize`. Supported languages are English, Russian, Simplified Chinese and Traditional Chinese. Unsupported host languages select English. A zero/missing host language uses `GetUserDefaultUILanguage()` before applying the same rule. The selection lasts until the plugin is reloaded.

| Language | Resource offset | Host language mapping | Dialog font |
| --- | --- | --- | --- |
| English | 0 | English and unsupported languages | Segoe UI |
| Russian | 1000 | Russian LANGIDs | Segoe UI |
| Simplified Chinese | 2000 | Mainland China, Singapore and neutral Simplified Chinese | Microsoft YaHei UI |
| Traditional Chinese | 3000 | Taiwan, Hong Kong, Macau and neutral Traditional Chinese | Microsoft JhengHei UI |

The settings/help/SSH-preview templates, offline help text and notification strings are in `src/resources.rc` (English) and its included `resources_ru.rc`, `resources_zh_cn.rc` and `resources_zh_tw.rc`. The resource compiler explicitly reads UTF-8. Each dialog/string uses its base English resource ID plus the selected language's offset, so selection does not depend on the Windows resource loader's preferred language. Control IDs and bindings are shared. The Chinese resources contain complete translations and are normal build inputs; no translation service or generator is needed to build.

Status, error, measurement and validation vocabulary is centralized in model.cpp and receives the selected language explicitly. Tooltip and manual-result formatting use that language too. Newly added groups/checks receive localized default names. First launch without a saved file creates empty LOCAL, WEB and SSH groups using the host language; adding defaults explicitly uses the current dialog language. Existing configured group/check names and labels are user data and are never translated or renamed automatically. SSH aliases are preserved as names, while preview controls, skip reasons and explanations use the selected UI language.

Configuration tags, `All` in the persistent file, UUIDs, protocol names, URLs and numeric error/status codes are not translated. The combo box displays `Все` in Russian while saving the same `All` token. Low-level parser/filesystem/OS exception details may remain in English; the surrounding load/save explanation is localized.

The live vocabulary lookup requires an explicit value for all four languages. Low-level technical exception details remain an intentional exception to full UI translation. Standard Windows file-picker buttons follow the OS language; its title/filter and the NetWatch preview follow the host language.

Automated tests instantiate the real settings/help/SSH-preview resources, verify Chinese resource completeness, captions, the localized All selection, selected SSH import and restart notices. The ABI host loads the actual DLL with all four `ITrafficMonitor` languages and verifies localized tooltips/descriptions without renaming user data; fresh Russian and Chinese defaults are also checked. Russian and both Chinese settings layouts were visually inspected in the standalone host, and Simplified Chinese help was opened, scrolled to its final sections and closed. A full DPI/theme/font-availability matrix and native-speaker review of Chinese phrasing remain open.
