# Localization

English is the default UI language regardless of the host language. Dialog text and UI notifications are stored in `src/resources.rc`, using stable identifiers from `resource.h` and explicit `LANGUAGE LANG_ENGLISH` resources. A future Russian translation can add matching dialog/string-table resources under `LANG_RUSSIAN`; control IDs, bindings, config IDs and probe logic do not need to change.

Status/error/measurement vocabulary is centralized in `health_text`, `error_text` and `measurement` in model.cpp. Validation messages are centralized in `validate`; formatting is in `describe`. To add full localization, replace those text lookups with a selected language catalog and translate the small set of settings/config error strings. Do not translate persistent file tags, UUIDs, HTTP status codes or Win32 error codes. Probe implementations return structured enums/codes and do not generate localized network-result strings.

Only English resources are included and verified in this version. Locale selection, Russian resources and a complete translated error catalog are not implemented.
