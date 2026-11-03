// Compile the real dialog controller into an isolated hidden test process.
#include <Windows.h>
#include <commdlg.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
static std::vector<std::wstring> notifications;
static std::vector<std::wstring> help_titles, help_contents;
static bool help_readonly{};
static std::filesystem::path ssh_fixture;
static bool cancel_picker{}, cancel_preview{}, ssh_preview_seen{};
static std::wstring ssh_preview_title;
static BOOL WINAPI inspect_file_picker(LPOPENFILENAMEW picker) {
    if (cancel_picker) return FALSE;
    wcscpy_s(picker->lpstrFile, picker->nMaxFile, ssh_fixture.c_str()); return TRUE;
}
static INT_PTR WINAPI inspect_help_dialog(HINSTANCE module, LPCWSTR resource, HWND parent, DLGPROC callback, LPARAM data);
static int WINAPI record_message_box(HWND, LPCWSTR text, LPCWSTR, UINT) {
    notifications.emplace_back(text); return IDOK;
}
#define MessageBoxW record_message_box
#define DialogBoxParamW inspect_help_dialog
#define GetOpenFileNameW inspect_file_picker
#include "../src/settings.cpp"
#undef MessageBoxW
#undef DialogBoxParamW
#undef GetOpenFileNameW
#include <iostream>

static void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static std::wstring cell(HWND window, int row, int column) {
    wchar_t value[4096]{}; ListView_GetItemText(GetDlgItem(window, IDC_CHECKS), row, column, value, 4096); return value;
}
static void live_status() {
    auto config = nw::initial_config(); config.groups.resize(1);
    for (int i = 0; i < 3; ++i) {
        nw::Check c; c.id = nw::new_id(); c.name = L"Node " + std::to_wstring(i); c.interval_ms = 1000; c.timeout_ms = 300;
        if (i == 1) c.target = L"bad host name";
        c.enabled = i != 2; config.groups[0].checks.push_back(c);
    }
    nw::Engine engine(config);
    nw::Dialog dialog{GetModuleHandleW(nullptr), engine, {}, {}, config, nw::visible_groups(config), nw::Language::Russian};
    HWND window = CreateDialogParamW(dialog.module, MAKEINTRESOURCEW(IDD_SETTINGS_RU), nullptr, nw::procedure, reinterpret_cast<LPARAM>(&dialog));
    require(window != nullptr, "Create monitoring table");
    HWND list = GetDlgItem(window, IDC_CHECKS);
    require(ListView_GetItemCount(list) == 3 && Header_GetItemCount(ListView_GetHeader(list)) == 5, "Monitoring table structure");
    require(cell(window, 0, 1) == L"Нет данных" && cell(window, 0, 2) == L"-" && cell(window, 0, 4) == L"-", "Untested row fabricated a measurement");
    require(cell(window, 2, 1) == L"Отключено", "Disabled check state");
    const auto until = nw::Clock::now() + std::chrono::seconds(7);
    while (nw::Clock::now() < until && (cell(window, 0, 1) != L"Доступно" || cell(window, 1, 1) != L"Недоступно")) {
        engine.dispatch(); Sleep(20); SendMessageW(window, WM_TIMER, 2, 0);
    }
    require(cell(window, 0, 1) == L"Доступно" && cell(window, 0, 3) == L"Успешно", "Table did not receive live ICMP success");
    require(cell(window, 0, 2).find(L"ICMP ") == 0 && cell(window, 0, 4).size() == 19, "Missing typed monitoring measurement/timestamp");
    require(cell(window, 1, 1) == L"Недоступно" && cell(window, 1, 2) == L"-", "DNS failures bypassed debounce or fabricated latency");
    require(cell(window, 1, 3).find(L"Ошибка DNS; код=") == 0 && cell(window, 1, 3).find(L"3/3") != std::wstring::npos, "Raw failure details not shown");
    const auto snapshot = engine.checks(config.groups[0].id);
    require(snapshot.size() == 3 && snapshot[0].history && snapshot[0].history->has_result && !snapshot[2].monitored, "Saved monitoring snapshot");
    require(engine.checks(nw::new_id()).empty(), "Unknown group leaked another group's results");
    dialog.text(IDC_CHECK_TARGET, L"192.0.2.9"); dialog.text(IDC_CHECK_INTERVAL, L"unfinished");
    SendMessageW(window, WM_TIMER, 2, 0);
    require(cell(window, 0, 1) == L"Не сохранено" && cell(window, 0, 2) == L"-" && cell(window, 0, 4) == L"-", "Unsaved target reused saved status");
    require(dialog.text(IDC_CHECK_INTERVAL) == L"unfinished" && dialog.check == 0, "Status timer captured or replaced unfinished editor");
    require(engine.configuration().groups[0].checks[0].target == L"127.0.0.1", "Status display changed saved configuration");
    const auto errors = notifications.size();
    ListView_SetItemState(list, 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    require(dialog.check == 0 && ListView_GetNextItem(list, -1, LVNI_SELECTED) == 0 && notifications.size() == errors + 1, "Invalid edit was lost on row selection");
    dialog.text(IDC_CHECK_TARGET, L"127.0.0.1"); dialog.text(IDC_CHECK_INTERVAL, L"1");
    SendMessageW(window, WM_TIMER, 2, 0);
    require(cell(window, 0, 1) == L"Доступно", "Reverted draft did not recover live status");
    ListView_SetItemState(list, 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    require(dialog.check == 1 && dialog.text(IDC_CHECK_TARGET) == L"bad host name", "Table selection failed to open check editor");
    dialog.command(IDC_CHECK_ADD, BN_CLICKED);
    require(cell(window, 3, 1) == L"Не сохранено", "New draft check claimed a monitoring result");
    dialog.command(IDC_TEST, BN_CLICKED);
    for (int i = 0; i < 100 && dialog.manual; ++i) { Sleep(10); dialog.poll(); }
    require(!dialog.manual, "Manual test did not finish");
    SendMessageW(window, WM_TIMER, 2, 0);
    require(cell(window, 3, 1) == L"Не сохранено" && engine.checks(config.groups[0].id).size() == 3, "Manual draft test entered monitoring table/history");
    require(engine.checks(config.groups[0].id)[0].history->last.updated == snapshot[0].history->last.updated, "Manual test rewrote monitoring history");
    Sleep(3100); SendMessageW(window, WM_TIMER, 2, 0);
    require(cell(window, 0, 1) == L"Устарело" && cell(window, 0, 3) == L"Успешно", "Expired monitoring result still appears current");
    engine.apply(config); SendMessageW(window, WM_TIMER, 2, 0);
    require(cell(window, 0, 1) == L"Нет данных" && cell(window, 0, 4) == L"-", "Table retained results from the previous generation");
    auto changed = config; changed.groups[0].checks[0].target = L"changed.example.invalid";
    engine.apply(changed); SendMessageW(window, WM_TIMER, 2, 0);
    require(cell(window, 0, 1) == L"Не сохранено", "Same UUID hid a changed saved destination");
    DestroyWindow(window);
    for (int i = 0; i < 200 && engine.active(); ++i) Sleep(10);
    require(engine.active() == 0, "Monitoring workers did not drain after settings close");
    auto disabled = config; disabled.groups[0].enabled = false; engine.apply(disabled);
    for (const auto& item : engine.checks(config.groups[0].id)) require(!item.monitored && !item.running && !item.history, "Generation reset retained disabled group's history");
}
static INT_PTR WINAPI inspect_help_dialog(HINSTANCE module, LPCWSTR resource, HWND parent, DLGPROC callback, LPARAM data) {
    HWND window = CreateDialogParamW(module, resource, parent, callback, data);
    if (!window) return -1;
    const auto id = reinterpret_cast<ULONG_PTR>(resource);
    if (id % 1000 == IDD_SSH_IMPORT) {
        ssh_preview_seen = true;
        wchar_t title[128]{}; GetWindowTextW(window, title, 128); ssh_preview_title = title;
        require(ListView_GetItemCount(GetDlgItem(window, IDC_SSH_HOSTS)) == 3, "Real SSH preview rows");
        auto* preview = reinterpret_cast<nw::SshDialog*>(data);
        require(preview->issues[1] == nw::SshIssue::Proxy, "Preview missing proxy explanation");
        ListView_SetCheckState(GetDlgItem(window, IDC_SSH_HOSTS), 2, FALSE);
        // Even a user-checked skipped row must not be imported.
        ListView_SetCheckState(GetDlgItem(window, IDC_SSH_HOSTS), 1, TRUE);
        SendMessageW(window, WM_COMMAND, cancel_preview ? IDCANCEL : IDOK, 0);
        DestroyWindow(window); return cancel_preview ? IDCANCEL : IDOK;
    }
    wchar_t title[128]{}; GetWindowTextW(window, title, 128); help_titles.emplace_back(title);
    HWND text = GetDlgItem(window, IDC_HELP_TEXT);
    std::wstring content(static_cast<size_t>(GetWindowTextLengthW(text)) + 1, 0);
    GetWindowTextW(text, content.data(), static_cast<int>(content.size())); content.pop_back();
    help_contents.push_back(std::move(content));
    help_readonly = (GetWindowLongPtrW(text, GWL_STYLE) & (ES_READONLY | WS_VSCROLL)) == (ES_READONLY | WS_VSCROLL);
    SendMessageW(window, WM_COMMAND, IDOK, 0); DestroyWindow(window); return IDOK;
}
int main() {
    try {
        live_status(); notifications.clear();
        {
            nw::Engine live(nw::initial_config());
            const auto folder = std::filesystem::temp_directory_path() / nw::new_id();
            const auto exported = nw::visible_groups(live.configuration());
            nw::Dialog save{GetModuleHandleW(nullptr), live, folder / L"NetWatch.conf", {}, live.configuration(), exported};
            HWND save_window = CreateDialogParamW(save.module, MAKEINTRESOURCEW(IDD_SETTINGS), nullptr, nw::procedure, reinterpret_cast<LPARAM>(&save));
            require(save_window != nullptr, "Create save regression dialog");
            wchar_t heading[128]{}; LVCOLUMNW header{}; header.mask = LVCF_TEXT; header.pszText = heading; header.cchTextMax = 128;
            ListView_GetColumn(GetDlgItem(save_window, IDC_CHECKS), 1, &header);
            require(std::wstring(heading) == L"State", "English monitoring table header");
            require(save.text(IDOK) == L"Save" && save.text(IDC_LIMIT) == L"All", "Explicit English resource selection");
            save.command(IDC_OPEN_HELP, BN_CLICKED);
            require(help_titles.back() == L"NetWatch help" && help_contents.back().find(L"IF A CHECK FAILS") != std::wstring::npos && help_readonly, "English help resource and full scrollable content");
            require(help_contents.back().find(L"column widths can be adjusted.") != std::wstring::npos, "Monitoring help was truncated");
            save.command(IDC_CHECK_ADD, BN_CLICKED);
            save.command(IDOK, BN_CLICKED);
            require(save.changed && live.configuration().groups[0].checks.size() == 1, "Save applies new check");
            require(notifications.empty(), "Check-only save incorrectly requests a host restart");
            require(nw::load_config(save.path).groups[0].checks.size() == 1, "Check-only save was not persisted");
            DestroyWindow(save_window);
            nw::Dialog layout{save.module, live, save.path, {}, live.configuration(), exported, nw::Language::Russian};
            HWND layout_window = CreateDialogParamW(layout.module, MAKEINTRESOURCEW(IDD_SETTINGS_RU), nullptr, nw::procedure, reinterpret_cast<LPARAM>(&layout));
            require(layout_window != nullptr, "Create Russian dialog resource");
            wchar_t title[128]{}; GetWindowTextW(layout_window, title, 128);
            require(std::wstring(title) == L"Настройки NetWatch" && layout.text(IDOK) == L"Сохранить", "Russian dialog caption/buttons");
            require(layout.text(IDC_LIMIT) == L"Все", "Localized All selection");
            layout.text(IDC_CHECK_TIMEOUT, L"unfinished");
            layout.command(IDC_OPEN_HELP, BN_CLICKED);
            require(help_titles.back() == L"Справка NetWatch" && help_contents.back().find(L"ЕСЛИ ПРОВЕРКА НЕ ПРОШЛА") != std::wstring::npos && help_readonly, "Russian help resource and full scrollable content");
            require(help_contents.back().find(L"ширину столбцов можно менять.") != std::wstring::npos, "Russian monitoring help was truncated or corrupted");
            require(layout.text(IDC_CHECK_TIMEOUT) == L"unfinished" && !layout.changed && live.active() == 0, "Help captured invalid draft or started probes");
            const auto opened = help_contents.size(); SendMessageW(layout_window, WM_HELP, 0, 0);
            require(help_contents.size() == opened + 1 && help_titles.back() == L"Справка NetWatch", "F1 help route");
            layout.text(IDC_CHECK_TIMEOUT, L"2000");
            layout.text(IDC_GROUP_LABEL, L"ДОМ"); layout.command(IDOK, BN_CLICKED);
            require(layout.changed && notifications.size() == 1 && notifications.back().find(L"Перезапустите") != std::wstring::npos, "Display save must request restart in Russian");
            require(nw::load_config(save.path).groups[0].label == L"ДОМ", "Display save was not persisted");
            DestroyWindow(layout_window);
            nw::Dialog reopened{save.module, live, save.path, {}, live.configuration(), exported, nw::Language::Russian};
            HWND reopened_window = CreateDialogParamW(reopened.module, MAKEINTRESOURCEW(IDD_SETTINGS_RU), nullptr, nw::procedure, reinterpret_cast<LPARAM>(&reopened));
            require(reopened_window != nullptr, "Reopen settings before restart");
            reopened.command(IDOK, BN_CLICKED);
            require(notifications.size() == 2, "Pending display changes lost after reopening settings");
            DestroyWindow(reopened_window);
            nw::Dialog restarted{save.module, live, save.path, {}, live.configuration(), nw::visible_groups(live.configuration()), nw::Language::Russian};
            HWND restarted_window = CreateDialogParamW(restarted.module, MAKEINTRESOURCEW(IDD_SETTINGS_RU), nullptr, nw::procedure, reinterpret_cast<LPARAM>(&restarted));
            require(restarted_window != nullptr, "Create restarted dialog");
            restarted.command(IDC_CHECK_ADD, BN_CLICKED);
            require(restarted.text(IDC_CHECK_NAME) == L"Новая проверка", "Localized check default");
            restarted.command(IDC_TEST, BN_CLICKED);
            for (int i = 0; i < 100 && restarted.manual; ++i) { Sleep(20); restarted.poll(); }
            require(restarted.text(IDC_RESULT).find(L"Успешно") != std::wstring::npos, "Russian manual result");
            restarted.command(IDOK, BN_CLICKED);
            require(notifications.size() == 2, "Restarted host still requests restart on check-only save");
            DestroyWindow(restarted_window);
            std::filesystem::remove_all(folder);
        }
        auto editing = nw::initial_config(); editing.groups.resize(1);
        nw::Engine engine(editing);
        nw::Dialog dialog{GetModuleHandleW(nullptr), engine, {}, {}, engine.configuration(), nw::visible_groups(engine.configuration())};
        HWND window = CreateDialogParamW(dialog.module, MAKEINTRESOURCEW(IDD_SETTINGS), nullptr, nw::procedure, reinterpret_cast<LPARAM>(&dialog));
        require(window != nullptr, "Create real dialog resource");
        auto id = dialog.draft.groups[0].id;
        dialog.text(IDC_GROUP_NAME, L"Renamed"); dialog.text(IDC_GROUP_LABEL, L"HOME");
        dialog.command(IDC_GROUP_ADD, BN_CLICKED);
        require(dialog.draft.groups.size() == 2 && dialog.draft.groups[0].name == L"Renamed", "UI add and rename");
        dialog.command(IDC_GROUP_UP, BN_CLICKED);
        require(dialog.group == 0 && dialog.draft.groups[1].id == id, "UI reorder keeps identities");
        dialog.command(IDC_GROUP_DELETE, BN_CLICKED);
        require(dialog.draft.groups.size() == 1 && dialog.draft.groups[0].id == id, "UI delete");
        dialog.command(IDC_CHECK_ADD, BN_CLICKED);
        require(dialog.draft.groups[0].checks.size() == 1, "UI add check");
        dialog.text(IDC_CHECK_NAME, L"Loopback draft"); dialog.text(IDC_CHECK_INTERVAL, L"2");
        dialog.checked(IDC_GROUP_VISIBLE, false); dialog.checked(IDC_GROUP_ENABLED, true); dialog.text(IDC_LIMIT, L"0"); dialog.capture();
        require(dialog.draft.max_visible == 0 && !dialog.draft.groups[0].visible && dialog.draft.groups[0].enabled, "UI independent visibility/monitoring and limit");
        require(dialog.draft.groups[0].checks[0].interval_ms == 2000, "UI interval unit conversion");
        dialog.command(IDC_TEST, BN_CLICKED);
        require(dialog.manual != nullptr, "UI manual worker");
        for (int i = 0; i < 100 && dialog.manual; ++i) { Sleep(20); dialog.poll(); }
        require(dialog.text(IDC_RESULT).find(L"Success") != std::wstring::npos, "UI manual result poll");
        require(engine.configuration().groups[0].checks.empty(), "Draft test changed live config");
        dialog.text(IDC_CHECK_TIMEOUT, L"0"); bool rejected = false;
        try { dialog.capture(); } catch (...) { rejected = true; }
        require(rejected, "UI invalid timeout accepted");
        dialog.text(IDC_CHECK_TIMEOUT, L"2000"); dialog.capture();
        dialog.command(IDC_CHECK_DELETE, BN_CLICKED); require(dialog.draft.groups[0].checks.empty(), "UI delete check");
        // A pending manual result owns no HWND. Destroying its consumer is safe.
        dialog.command(IDC_CHECK_ADD, BN_CLICKED); dialog.text(IDC_CHECK_TARGET, L"192.0.2.1"); dialog.text(IDC_CHECK_TIMEOUT, L"300");
        Sleep(1000); dialog.command(IDC_TEST, BN_CLICKED); require(dialog.manual != nullptr, "UI pending test");
        DestroyWindow(window); require(!dialog.manual, "Dialog retained manual state after destruction");
        for (int i = 0; i < 200 && engine.active(); ++i) Sleep(10);
        require(engine.active() == 0, "Pending dialog probe did not drain");
        {
            const auto folder = std::filesystem::temp_directory_path() / nw::new_id();
            std::filesystem::create_directories(folder); ssh_fixture = folder / L"ssh-config";
            { std::ofstream file(ssh_fixture); file << "Host imported-a\n HostName 192.0.2.40\n Port 2222\nHost jump\n ProxyJump gateway\nHost imported-b\n HostName 192.0.2.41\n"; }
            auto one = nw::initial_config(); one.groups.resize(1); one.groups[0].name = L"Custom local";
            nw::Engine import_engine(one);
            nw::Dialog imports{GetModuleHandleW(nullptr), import_engine, folder / L"NetWatch.conf", {}, one, nw::visible_groups(one), nw::Language::Russian};
            HWND import_window = CreateDialogParamW(imports.module, MAKEINTRESOURCEW(IDD_SETTINGS_RU), nullptr, nw::procedure, reinterpret_cast<LPARAM>(&imports));
            require(import_window != nullptr, "Create import settings");
            const auto local_id = imports.draft.groups[0].id;
            imports.command(IDC_DEFAULT_GROUPS, BN_CLICKED); imports.command(IDC_DEFAULT_GROUPS, BN_CLICKED);
            require(imports.draft.groups.size() == 3 && imports.draft.groups[0].id == local_id && imports.draft.groups[0].name == L"Custom local", "UI defaults are additive and repeatable");
            require(import_engine.configuration().groups.size() == 1, "Default button changed live configuration");
            cancel_picker = true; imports.command(IDC_IMPORT_SSH, BN_CLICKED);
            require(!ssh_preview_seen && imports.draft.groups[2].checks.empty(), "Canceled picker changed draft");
            cancel_picker = false; cancel_preview = true; imports.command(IDC_IMPORT_SSH, BN_CLICKED);
            require(ssh_preview_seen && imports.draft.groups[2].checks.empty(), "Canceled preview changed draft");
            cancel_preview = false; imports.command(IDC_IMPORT_SSH, BN_CLICKED);
            require(imports.draft.groups[2].checks.size() == 1 && imports.draft.groups[2].checks[0].name == L"imported-a" && imports.draft.groups[2].checks[0].port == 2222, "Selected TCP hosts did not reach draft");
            require(import_engine.configuration().groups.size() == 1 && !std::filesystem::exists(imports.path), "Import wrote/applied before Save");
            const auto imported_id = imports.draft.groups[2].checks[0].id;
            imports.command(IDC_IMPORT_SSH, BN_CLICKED);
            require(imports.draft.groups[2].checks.size() == 1 && imports.draft.groups[2].checks[0].id == imported_id, "UI reimport duplicated or replaced check");
            imports.command(IDOK, BN_CLICKED);
            require(import_engine.configuration().groups[2].checks.size() == 1 && nw::load_config(imports.path).groups[2].checks[0].id == imported_id, "Import Save lost TCP check/identity");
            DestroyWindow(import_window);
            for (const auto language : {nw::Language::ChineseSimplified, nw::Language::ChineseTraditional}) {
                const bool traditional = language == nw::Language::ChineseTraditional;
                nw::Engine localized_engine(nw::initial_config(language));
                nw::Dialog localized{GetModuleHandleW(nullptr), localized_engine, folder / (traditional ? L"tw.conf" : L"cn.conf"), {}, localized_engine.configuration(), nw::visible_groups(localized_engine.configuration()), language};
                HWND localized_window = CreateDialogParamW(localized.module, MAKEINTRESOURCEW(IDD_SETTINGS + nw::resource_offset(language)), nullptr, nw::procedure, reinterpret_cast<LPARAM>(&localized));
                require(localized_window != nullptr, "Create Chinese settings resource");
                wchar_t heading[128]{}; LVCOLUMNW header{}; header.mask = LVCF_TEXT; header.pszText = heading; header.cchTextMax = 128;
                ListView_GetColumn(GetDlgItem(localized_window, IDC_CHECKS), 1, &header);
                require(std::wstring(heading) == (traditional ? L"狀態" : L"状态"), "Chinese monitoring table header");
                require(localized.text(IDOK) == (traditional ? L"儲存" : L"保存") && localized.text(IDC_LIMIT) == L"全部", "Chinese buttons and All");
                for (UINT string_id = IDS_INVALID_NUMBER; string_id <= IDS_SSH_ERROR; ++string_id) require(!localized.string(string_id).empty(), "Missing localized resource string");
                localized.command(IDC_OPEN_HELP, BN_CLICKED);
                require(help_titles.back() == (traditional ? L"NetWatch 說明" : L"NetWatch 帮助") && help_contents.back().find(traditional ? L"預設群組" : L"标准分组") != std::wstring::npos, "Chinese help completeness");
                require(help_contents.back().find(traditional ? L"可調整欄寬。" : L"可调整列宽。") != std::wstring::npos, "Chinese monitoring help was truncated or corrupted");
                localized.command(IDC_IMPORT_SSH, BN_CLICKED);
                require(ssh_preview_title == (traditional ? L"匯入 OpenSSH 主機" : L"导入 OpenSSH 主机") && localized.draft.groups[2].checks.size() == 1, "Chinese SSH preview and selected import");
                require(cell(localized_window, 0, 1) == (traditional ? L"未儲存" : L"未保存"), "Chinese draft monitoring state");
                localized.text(IDC_GROUP_LABEL, L"SSH2"); localized.command(IDOK, BN_CLICKED);
                require(notifications.back().find(traditional ? L"重新啟動" : L"重启") != std::wstring::npos, "Chinese restart notice");
                require(!nw::load_config(localized.path).max_visible, "Chinese All persisted as internal All");
                DestroyWindow(localized_window);
            }
            std::filesystem::remove_all(folder);
        }
        std::cout << "PASS real settings controller: group/check editing, ordering, IDs, limit, validation, draft tests and close during probe\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL settings: " << e.what() << '\n'; return 1; }
}
