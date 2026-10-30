# Localization

NetWatch reads `ITrafficMonitor::GetLanguageId()` during `OnInitialize`. Russian LANGIDs select Russian; English and unsupported host languages select English. A zero/missing host language uses `GetUserDefaultUILanguage()` before applying the same rule. The selection lasts until the plugin is reloaded.

The dialog templates and notification strings are in `src/resources.rc` (English) and its included `src/resources_ru.rc` (Russian). The resource compiler explicitly reads UTF-8. Each template has a separate resource ID; Russian string IDs are the corresponding English ID plus 1000, so selecting the host language does not depend on the Windows resource loader's preferred language. Control IDs and bindings are shared.

Status, error, measurement and validation vocabulary is centralized in model.cpp and receives the selected language explicitly. Tooltip and manual-result formatting use that language too. Newly added groups/checks receive localized default names. Existing configured group/check names and labels are user data and are never translated or renamed automatically. The initial empty LOCAL group retains its existing English name.

Configuration tags, `All` in the persistent file, UUIDs, protocol names, URLs and numeric error/status codes are not translated. The combo box displays `Все` in Russian while saving the same `All` token. Low-level parser/filesystem/OS exception details may remain in English; the surrounding load/save explanation is localized.

Automated tests instantiate both real dialog resources, verify captions, controls, the localized All selection and manual result, and exercise conditional Save notices through the real controller. The ABI host loads the actual DLL with Russian and English `ITrafficMonitor` implementations and verifies localized tooltips/descriptions without renaming user data. Visual clipping and the full DPI/theme matrix still require desktop inspection.
