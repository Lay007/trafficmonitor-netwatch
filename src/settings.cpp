#include "settings.h"
#include "config.h"
#include "resource.h"
#include "ssh_import.h"
#include <commctrl.h>
#include <commdlg.h>
#include <stdexcept>
#include <iomanip>
#include <sstream>
#include <array>

namespace nw {
namespace {
std::wstring resource_string(HINSTANCE module, Language language, UINT id) {
    const wchar_t* text{};
    const auto length = LoadStringW(module, id + resource_offset(language), reinterpret_cast<LPWSTR>(&text), 0);
    return length ? std::wstring(text, length) : std::wstring{};
}
std::filesystem::path user_home() {
    const auto length = GetEnvironmentVariableW(L"USERPROFILE", nullptr, 0);
    if (!length) throw std::runtime_error("USERPROFILE is unavailable");
    std::wstring value(length, 0);
    const auto copied = GetEnvironmentVariableW(L"USERPROFILE", value.data(), length);
    if (!copied || copied >= length) throw std::runtime_error("Cannot read USERPROFILE");
    value.resize(copied); return value;
}
struct SshDialog {
    HINSTANCE module;
    Language language;
    Config config;
    std::filesystem::path path;
    std::vector<SshHost> hosts;
    HWND window{};
    int group{-1};
    bool accepted{};
    std::vector<SshIssue> issues;
    void refresh() {
        const auto index = static_cast<size_t>(SendDlgItemMessageW(window, IDC_SSH_GROUP, CB_GETCURSEL, 0, 0));
        issues = ssh_import_status(index < config.groups.size() ? config.groups[index] : Group{}, hosts);
        HWND list = GetDlgItem(window, IDC_SSH_HOSTS); ListView_DeleteAllItems(list);
        for (size_t i = 0; i < hosts.size(); ++i) {
            LVITEMW item{}; item.mask = LVIF_TEXT; item.iItem = static_cast<int>(i); item.pszText = const_cast<wchar_t*>(hosts[i].alias.c_str());
            ListView_InsertItem(list, &item);
            ListView_SetItemText(list, item.iItem, 1, const_cast<wchar_t*>(hosts[i].target.c_str()));
            auto port = std::to_wstring(hosts[i].port); ListView_SetItemText(list, item.iItem, 2, port.data());
            ListView_SetItemText(list, item.iItem, 3, const_cast<wchar_t*>(ssh_issue_text(issues[i], language)));
            ListView_SetCheckState(list, item.iItem, issues[i] == SshIssue::None);
        }
    }
    void initialize() {
        SetDlgItemTextW(window, IDC_SSH_PATH, path.c_str());
        HWND list = GetDlgItem(window, IDC_SSH_HOSTS);
        ListView_SetExtendedListViewStyle(list, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        RECT bounds{}; GetClientRect(list, &bounds);
        const wchar_t* titles[] = {
            translated(language, L"Host alias", L"Псевдоним Host", L"Host 别名", L"Host 別名"), translated(language, L"Address", L"Адрес", L"地址", L"位址"),
            translated(language, L"Port", L"Порт", L"端口", L"連接埠"), translated(language, L"Import status", L"Статус импорта", L"导入状态", L"匯入狀態")
        };
        const int widths[] = {20, 32, 8, 40};
        for (int i = 0; i < 4; ++i) {
            LVCOLUMNW column{}; column.mask = LVCF_TEXT | LVCF_WIDTH; column.pszText = const_cast<wchar_t*>(titles[i]);
            column.cx = (bounds.right - 24) * widths[i] / 100; ListView_InsertColumn(list, i, &column);
        }
        for (size_t i = 0; i < config.groups.size(); ++i) {
            SendDlgItemMessageW(window, IDC_SSH_GROUP, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(config.groups[i].name.c_str()));
            if (CompareStringOrdinal(config.groups[i].label.c_str(), -1, L"SSH", -1, TRUE) == CSTR_EQUAL) group = static_cast<int>(i);
        }
        const auto label = resource_string(module, language, IDS_SSH_NEW_GROUP);
        SendDlgItemMessageW(window, IDC_SSH_GROUP, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        if (group < 0) group = static_cast<int>(config.groups.size());
        SendDlgItemMessageW(window, IDC_SSH_GROUP, CB_SETCURSEL, group, 0); refresh();
    }
    void apply() {
        std::vector<SshHost> selected;
        for (size_t i = 0; i < hosts.size(); ++i)
            if (issues[i] == SshIssue::None && ListView_GetCheckState(GetDlgItem(window, IDC_SSH_HOSTS), static_cast<int>(i))) selected.push_back(hosts[i]);
        if (selected.empty()) {
            MessageBoxW(window, resource_string(module, language, IDS_SSH_NONE).c_str(), L"NetWatch", MB_OK | MB_ICONINFORMATION); return;
        }
        auto next = config;
        group = static_cast<int>(SendDlgItemMessageW(window, IDC_SSH_GROUP, CB_GETCURSEL, 0, 0));
        if (group < 0 || static_cast<size_t>(group) > next.groups.size()) throw std::runtime_error("Select an SSH import group");
        if (static_cast<size_t>(group) == next.groups.size()) {
            Group added; added.id = new_id(); added.name = added.label = L"SSH"; next.groups.push_back(std::move(added));
        }
        append_ssh_hosts(next, next.groups.at(group).id, selected);
        config = std::move(next); accepted = true; EndDialog(window, IDOK);
    }
};
INT_PTR CALLBACK ssh_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept {
    auto* self = reinterpret_cast<SshDialog*>(GetWindowLongPtrW(window, DWLP_USER));
    try {
        if (message == WM_INITDIALOG) {
            self = reinterpret_cast<SshDialog*>(lparam); self->window = window;
            SetWindowLongPtrW(window, DWLP_USER, reinterpret_cast<LONG_PTR>(self)); self->initialize(); return TRUE;
        }
        if (!self) return FALSE;
        if (message == WM_COMMAND) {
            if (LOWORD(wparam) == IDCANCEL) { EndDialog(window, IDCANCEL); return TRUE; }
            if (LOWORD(wparam) == IDOK) { self->apply(); return TRUE; }
            if (LOWORD(wparam) == IDC_SSH_GROUP && HIWORD(wparam) == CBN_SELCHANGE) { self->refresh(); return TRUE; }
        }
        if (message == WM_CLOSE) { EndDialog(window, IDCANCEL); return TRUE; }
    } catch (const std::exception& error) { MessageBoxW(window, wide(error.what()).c_str(), L"NetWatch", MB_OK | MB_ICONERROR); }
    catch (...) { MessageBoxW(window, L"Unexpected error", L"NetWatch", MB_OK | MB_ICONERROR); }
    return FALSE;
}
INT_PTR CALLBACK help_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept {
    if (message == WM_INITDIALOG) {
        SetDlgItemTextW(window, IDC_HELP_TEXT, reinterpret_cast<const wchar_t*>(lparam)); return TRUE;
    }
    if (message == WM_CLOSE || (message == WM_COMMAND && (LOWORD(wparam) == IDOK || LOWORD(wparam) == IDCANCEL))) {
        EndDialog(window, IDOK); return TRUE;
    }
    return FALSE;
}
struct Dialog {
    HINSTANCE module;
    Engine& engine;
    std::filesystem::path path;
    std::wstring load_error;
    Config draft;
    const std::vector<Group> exported;
    Language language{Language::English};
    HWND window{};
    int group{-1}, check{-1};
    bool filling{}, changed{};
    std::shared_ptr<ManualResult> manual;
    Check tested;
    std::wstring string(UINT id) const {
        return resource_string(module, language, id);
    }
    void help() const {
        std::wstring content;
        for (UINT id = IDS_HELP_START; id <= IDS_HELP_DEFAULTS; ++id) {
            if (!content.empty()) content += L"\r\n\r\n";
            content += string(id);
        }
        const auto result = DialogBoxParamW(module, MAKEINTRESOURCEW(IDD_HELP + resource_offset(language)),
            window, help_procedure, reinterpret_cast<LPARAM>(content.c_str()));
        if (result == -1) throw std::runtime_error("Cannot create NetWatch help dialog");
    }
    void import_ssh() {
        const auto home = user_home();
        wchar_t filename[32768]{};
        const auto initial = home / L".ssh" / L"config";
        if (initial.wstring().size() < 32768) wcscpy_s(filename, initial.c_str());
        const auto title = string(IDS_SSH_SELECT);
        OPENFILENAMEW picker{}; picker.lStructSize = sizeof(picker); picker.hwndOwner = window;
        picker.lpstrFile = filename; picker.nMaxFile = 32768; picker.lpstrTitle = title.c_str();
        picker.lpstrFilter = translated(language, L"OpenSSH config\0config;*.conf\0All files\0*.*\0\0", L"Конфигурация OpenSSH\0config;*.conf\0Все файлы\0*.*\0\0", L"OpenSSH 配置\0config;*.conf\0所有文件\0*.*\0\0", L"OpenSSH 設定\0config;*.conf\0所有檔案\0*.*\0\0");
        picker.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetOpenFileNameW(&picker)) return;
        auto hosts = read_ssh_config(filename, home);
        if (hosts.empty()) { text(IDC_RESULT, string(IDS_SSH_EMPTY)); return; }
        SshDialog preview{module, language, draft, filename, std::move(hosts)};
        INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES}; InitCommonControlsEx(&controls);
        const auto result = DialogBoxParamW(module, MAKEINTRESOURCEW(IDD_SSH_IMPORT + resource_offset(language)),
            window, ssh_procedure, reinterpret_cast<LPARAM>(&preview));
        if (result == -1) throw std::runtime_error("Cannot create SSH import preview");
        if (preview.accepted) { draft = std::move(preview.config); group = preview.group; check = -1; fill_groups(); text(IDC_RESULT, string(IDS_SSH_ADDED)); }
    }
    void text(int id, const std::wstring& value) { SetDlgItemTextW(window, id, value.c_str()); }
    std::wstring text(int id) const {
        HWND control = GetDlgItem(window, id);
        const int length = GetWindowTextLengthW(control);
        std::wstring value(static_cast<size_t>(length) + 1, 0);
        GetWindowTextW(control, value.data(), length + 1); value.resize(length); return value;
    }
    bool checked(int id) const { return IsDlgButtonChecked(window, id) == BST_CHECKED; }
    void checked(int id, bool value) { CheckDlgButton(window, id, value ? BST_CHECKED : BST_UNCHECKED); }
    void enable(int id, bool value) { EnableWindow(GetDlgItem(window, id), value); }
    unsigned number(int id, unsigned maximum) const {
        const auto value = text(id);
        if (value.empty() || value.find_first_not_of(L"0123456789") != std::wstring::npos) throw std::runtime_error(utf8(string(IDS_INVALID_NUMBER)));
        const auto n = std::stoull(value);
        if (n > maximum) throw std::runtime_error(utf8(string(IDS_INVALID_NUMBER)));
        return static_cast<unsigned>(n);
    }
    void capture() {
        if (group >= 0) {
            auto& g = draft.groups.at(group);
            g.name = text(IDC_GROUP_NAME); g.label = text(IDC_GROUP_LABEL);
            g.enabled = checked(IDC_GROUP_ENABLED); g.visible = checked(IDC_GROUP_VISIBLE);
            if (check >= 0) {
                auto& c = g.checks.at(check);
                c.name = text(IDC_CHECK_NAME); c.target = text(IDC_CHECK_TARGET);
                c.kind = static_cast<Kind>(SendDlgItemMessageW(window, IDC_CHECK_KIND, CB_GETCURSEL, 0, 0));
                c.port = c.kind == Kind::Tcp ? number(IDC_CHECK_PORT, 65535) : 80;
                c.interval_ms = number(IDC_CHECK_INTERVAL, 86400) * 1000;
                c.timeout_ms = number(IDC_CHECK_TIMEOUT, 30000); c.enabled = checked(IDC_CHECK_ENABLED);
            }
        }
        const auto limit = text(IDC_LIMIT);
        draft.max_visible = limit == string(IDS_ALL) ? std::nullopt : std::optional<unsigned>(number(IDC_LIMIT, 4096));
        const auto error = validate(draft, language);
        if (!error.empty()) throw std::runtime_error(utf8(error));
    }
    static bool same_check(const Check& a, const Check& b) {
        return a.id == b.id && a.name == b.name && a.target == b.target && a.kind == b.kind && a.port == b.port &&
            a.interval_ms == b.interval_ms && a.timeout_ms == b.timeout_ms && a.enabled == b.enabled;
    }
    bool editor_matches(const Check& c) const {
        return text(IDC_CHECK_NAME) == c.name && text(IDC_CHECK_TARGET) == c.target &&
            SendDlgItemMessageW(window, IDC_CHECK_KIND, CB_GETCURSEL, 0, 0) == static_cast<int>(c.kind) &&
            (c.kind != Kind::Tcp || text(IDC_CHECK_PORT) == std::to_wstring(c.port)) &&
            text(IDC_CHECK_INTERVAL) == std::to_wstring(c.interval_ms / 1000) &&
            text(IDC_CHECK_TIMEOUT) == std::to_wstring(c.timeout_ms) && checked(IDC_CHECK_ENABLED) == c.enabled;
    }
    void refresh_status() {
        if (group < 0) return;
        const auto& selected = draft.groups.at(group);
        const auto snapshot = engine.checks(selected.id);
        std::map<std::wstring, const CheckSnapshot*> by_id;
        for (const auto& item : snapshot) by_id.emplace(item.check.id, &item);
        HWND list = GetDlgItem(window, IDC_CHECKS);
        const auto now = Clock::now();
        for (size_t i = 0; i < selected.checks.size(); ++i) {
            const auto& c = selected.checks[i];
            const auto found = by_id.find(c.id);
            const auto* live = found == by_id.end() ? nullptr : found->second;
            const bool edited = !live || (static_cast<int>(i) == check ? !editor_matches(live->check) : !same_check(c, live->check)) ||
                (checked(IDC_GROUP_ENABLED) && live->check.enabled) != live->monitored;
            std::array<std::wstring, 5> cells{static_cast<int>(i) == check ? text(IDC_CHECK_NAME) : c.name, L"", L"-", L"-", L"-"};
            if (edited) cells[1] = translated(language, L"Unsaved", L"Не сохранено", L"未保存", L"未儲存");
            else {
                const auto* history = live->history ? &*live->history : nullptr;
                cells[1] = !live->monitored ? translated(language, L"Disabled", L"Отключено", L"已禁用", L"已停用") :
                    live->running ? translated(language, L"Checking...", L"Проверяется...", L"检测中...", L"檢查中...") :
                    history && stale(live->check, *history, now) ? translated(language, L"Stale", L"Устарело", L"已过期", L"已過期") : health_text(health(live->check, history, now), language);
                if (history && history->has_result) {
                    const auto& result = history->last;
                    cells[3] = error_text(result.error, language);
                    if (result.http_status) cells[3] += L"; HTTP " + std::to_wstring(result.http_status);
                    if (result.code) cells[3] += translated(language, L"; code=", L"; код=", L"; 错误码=", L"; 錯誤碼=") + std::to_wstring(result.code);
                    if (history->failures) cells[3] += L"; " + std::to_wstring(history->failures) + L"/3";
                    if (result.has_measurement) {
                        std::wostringstream duration; duration << std::fixed << std::setprecision(1) << result.milliseconds;
                        cells[2] = std::wstring(live->check.kind == Kind::Icmp ? L"ICMP " : live->check.kind == Kind::Tcp ? L"TCP " : L"HTTP ") + duration.str();
                    }
                    const auto time = std::chrono::system_clock::to_time_t(result.updated);
                    tm local{};
                    if (localtime_s(&local, &time) == 0) {
                        std::wostringstream updated; updated << std::put_time(&local, L"%Y-%m-%d %H:%M:%S"); cells[4] = updated.str();
                    }
                }
            }
            // Update cells in place so polling preserves selection, scroll and unfinished edits.
            for (int column = 0; column < static_cast<int>(cells.size()); ++column) {
                wchar_t previous[4096]{};
                ListView_GetItemText(list, static_cast<int>(i), column, previous, 4096);
                if (cells[column] != previous) ListView_SetItemText(list, static_cast<int>(i), column, cells[column].data());
            }
        }
    }
    void select_check(const NMLISTVIEW& change) {
        if (filling || !(change.uChanged & LVIF_STATE) || !(change.uNewState & LVIS_SELECTED) || (change.uOldState & LVIS_SELECTED)) return;
        capture(); check = change.iItem; fill_check(); refresh_status();
    }
    void fill_check() {
        filling = true;
        const bool selected = group >= 0 && check >= 0;
        for (int id : {IDC_CHECK_NAME, IDC_CHECK_KIND, IDC_CHECK_TARGET, IDC_CHECK_PORT, IDC_CHECK_INTERVAL, IDC_CHECK_TIMEOUT, IDC_CHECK_ENABLED, IDC_CHECK_DELETE, IDC_TEST}) enable(id, selected && (id != IDC_TEST || !manual));
        if (selected) {
            const auto& c = draft.groups.at(group).checks.at(check);
            text(IDC_CHECK_NAME, c.name); text(IDC_CHECK_TARGET, c.target);
            SendDlgItemMessageW(window, IDC_CHECK_KIND, CB_SETCURSEL, static_cast<int>(c.kind), 0);
            text(IDC_CHECK_PORT, std::to_wstring(c.port)); text(IDC_CHECK_INTERVAL, std::to_wstring(c.interval_ms / 1000)); text(IDC_CHECK_TIMEOUT, std::to_wstring(c.timeout_ms));
            checked(IDC_CHECK_ENABLED, c.enabled); enable(IDC_CHECK_PORT, c.kind == Kind::Tcp);
        } else {
            for (int id : {IDC_CHECK_NAME, IDC_CHECK_TARGET, IDC_CHECK_PORT, IDC_CHECK_INTERVAL, IDC_CHECK_TIMEOUT}) text(id, L"");
            checked(IDC_CHECK_ENABLED, false);
        }
        filling = false;
    }
    void fill_group() {
        filling = true;
        for (int id : {IDC_GROUP_NAME, IDC_GROUP_LABEL, IDC_GROUP_ENABLED, IDC_GROUP_VISIBLE, IDC_GROUP_DELETE, IDC_GROUP_UP, IDC_GROUP_DOWN, IDC_CHECK_ADD}) enable(id, group >= 0);
        HWND list = GetDlgItem(window, IDC_CHECKS); ListView_DeleteAllItems(list);
        if (group >= 0) {
            const auto& g = draft.groups.at(group);
            text(IDC_GROUP_NAME, g.name); text(IDC_GROUP_LABEL, g.label);
            checked(IDC_GROUP_ENABLED, g.enabled); checked(IDC_GROUP_VISIBLE, g.visible);
            for (size_t i = 0; i < g.checks.size(); ++i) {
                LVITEMW row{}; row.mask = LVIF_TEXT; row.iItem = static_cast<int>(i); row.pszText = const_cast<wchar_t*>(g.checks[i].name.c_str());
                ListView_InsertItem(list, &row);
            }
            check = g.checks.empty() ? -1 : (std::max)(0, (std::min)(check, static_cast<int>(g.checks.size()) - 1));
        } else {
            text(IDC_GROUP_NAME, L""); text(IDC_GROUP_LABEL, L""); checked(IDC_GROUP_ENABLED, false); checked(IDC_GROUP_VISIBLE, false); check = -1;
        }
        if (check >= 0) {
            ListView_SetItemState(list, check, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(list, check, FALSE);
        }
        filling = false; fill_check(); refresh_status();
    }
    void fill_groups() {
        filling = true;
        SendDlgItemMessageW(window, IDC_GROUPS, LB_RESETCONTENT, 0, 0);
        for (const auto& g : draft.groups) SendDlgItemMessageW(window, IDC_GROUPS, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(g.name.c_str()));
        group = draft.groups.empty() ? -1 : (std::max)(0, (std::min)(group, static_cast<int>(draft.groups.size()) - 1));
        SendDlgItemMessageW(window, IDC_GROUPS, LB_SETCURSEL, group, 0);
        filling = false; fill_group();
    }
    void initialize() {
        HWND list = GetDlgItem(window, IDC_CHECKS);
        ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
        const wchar_t* titles[] = {
            translated(language, L"Name", L"Название", L"名称", L"名稱"), translated(language, L"State", L"Состояние", L"状态", L"狀態"),
            translated(language, L"Time, ms", L"Время, мс", L"耗时，毫秒", L"耗時，毫秒"),
            translated(language, L"Last result", L"Последний результат", L"上次结果", L"上次結果"), translated(language, L"Checked", L"Проверено", L"检测时间", L"檢查時間")
        };
        RECT bounds{}; GetClientRect(list, &bounds); const int widths[] = {24, 16, 12, 28, 20};
        for (int i = 0; i < 5; ++i) {
            LVCOLUMNW column{}; column.mask = LVCF_TEXT | LVCF_WIDTH; column.pszText = const_cast<wchar_t*>(titles[i]);
            column.cx = (bounds.right - GetSystemMetrics(SM_CXVSCROLL)) * widths[i] / 100; ListView_InsertColumn(list, i, &column);
        }
        const auto all = string(IDS_ALL);
        for (const auto value : {all.c_str(), L"0", L"1", L"2", L"4", L"8"}) SendDlgItemMessageW(window, IDC_LIMIT, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
        for (const auto value : {L"ICMP", L"TCP", L"HTTP / HTTPS"}) SendDlgItemMessageW(window, IDC_CHECK_KIND, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
        text(IDC_LIMIT, draft.max_visible ? std::to_wstring(*draft.max_visible) : all);
        SendDlgItemMessageW(window, IDC_GROUP_NAME, EM_SETLIMITTEXT, 128, 0);
        SendDlgItemMessageW(window, IDC_GROUP_LABEL, EM_SETLIMITTEXT, 24, 0);
        SendDlgItemMessageW(window, IDC_CHECK_NAME, EM_SETLIMITTEXT, 128, 0);
        SendDlgItemMessageW(window, IDC_CHECK_TARGET, EM_SETLIMITTEXT, 2048, 0);
        fill_groups();
        SetTimer(window, 2, 500, nullptr);
        if (!load_error.empty()) text(IDC_RESULT, load_error + L"\r\n" + string(IDS_LOAD_ERROR));
    }
    void command(int id, int notification) {
        if (filling) return;
        if (id == IDC_OPEN_HELP && notification == BN_CLICKED) { help(); return; }
        if (id == IDCANCEL) { EndDialog(window, IDCANCEL); return; }
        if (id == IDC_CHECK_KIND && notification == CBN_SELCHANGE) {
            enable(IDC_CHECK_PORT, SendDlgItemMessageW(window, IDC_CHECK_KIND, CB_GETCURSEL, 0, 0) == static_cast<int>(Kind::Tcp)); return;
        }
        if (id == IDC_GROUPS && notification == LBN_SELCHANGE) {
            const int next = static_cast<int>(SendDlgItemMessageW(window, IDC_GROUPS, LB_GETCURSEL, 0, 0));
            capture(); group = next; check = -1; fill_groups(); return;
        }
        if (notification != BN_CLICKED) return;
        if (id == IDC_GROUP_DELETE && group >= 0) {
            draft.groups.erase(draft.groups.begin() + group); check = -1; fill_groups(); return;
        }
        if (id == IDC_CHECK_DELETE && group >= 0 && check >= 0) {
            auto& checks = draft.groups.at(group).checks; checks.erase(checks.begin() + check); fill_group(); return;
        }
        if (id != IDOK && id != IDC_GROUP_ADD && id != IDC_GROUP_UP && id != IDC_GROUP_DOWN && id != IDC_CHECK_ADD && id != IDC_TEST && id != IDC_IMPORT_SSH && id != IDC_DEFAULT_GROUPS) return;
        capture();
        if (id == IDC_DEFAULT_GROUPS) {
            add_default_groups(draft, language); fill_groups(); text(IDC_RESULT, string(IDS_DEFAULTS_ADDED));
        } else if (id == IDC_IMPORT_SSH) {
            try { import_ssh(); }
            catch (const std::exception& error) { throw std::runtime_error(utf8(string(IDS_SSH_ERROR)) + error.what()); }
        } else if (id == IDC_GROUP_ADD) {
            Group g; g.id = new_id(); g.name = string(IDS_NEW_GROUP); draft.groups.push_back(g); group = static_cast<int>(draft.groups.size()) - 1; check = -1; fill_groups();
        } else if ((id == IDC_GROUP_UP || id == IDC_GROUP_DOWN) && group >= 0) {
            const int next = group + (id == IDC_GROUP_UP ? -1 : 1);
            if (next >= 0 && static_cast<size_t>(next) < draft.groups.size()) { std::swap(draft.groups[group], draft.groups[next]); group = next; fill_groups(); }
        } else if (id == IDC_CHECK_ADD && group >= 0) {
            Check c; c.id = new_id(); c.name = string(IDS_NEW_CHECK); auto& checks = draft.groups.at(group).checks; checks.push_back(c); check = static_cast<int>(checks.size()) - 1; fill_group();
        } else if (id == IDC_TEST && group >= 0 && check >= 0 && !manual) {
            tested = draft.groups.at(group).checks.at(check);
            auto output = std::make_shared<ManualResult>();
            if (!engine.manual(tested, output)) { text(IDC_RESULT, string(IDS_BUSY)); return; }
            manual = std::move(output); text(IDC_RESULT, string(IDS_RUNNING)); enable(IDC_TEST, false); SetTimer(window, 1, 100, nullptr);
        } else if (id == IDOK) {
            try { save_config(path, draft); }
            catch (const std::exception& error) { throw std::runtime_error(utf8(string(IDS_SAVE_ERROR)) + error.what()); }
            engine.apply(draft); changed = true;
            if (display_changed(exported, draft)) MessageBoxW(window, string(IDS_RESTART).c_str(), L"NetWatch", MB_OK | MB_ICONINFORMATION);
            EndDialog(window, IDOK);
        }
    }
    void poll() {
        if (!manual) return;
        Result result;
        { std::lock_guard<std::mutex> lock(manual->mutex); if (!manual->done) return; result = manual->result; }
        std::wostringstream duration;
        duration << std::fixed << std::setprecision(1) << result.milliseconds;
        auto message = tested.name + L": " + error_text(result.error, language) + L"\r\n" + measurement(tested.kind, language) + L"=";
        message += result.has_measurement ? duration.str() + translated(language, L" ms", L" мс", L" ms", L" ms") : translated(language, L"n/a", L"нет данных", L"无数据", L"無資料");
        if (result.http_status) message += L"; HTTP " + std::to_wstring(result.http_status);
        if (result.code) message += translated(language, L"; code=", L"; код=", L"; 错误码=", L"; 錯誤碼=") + std::to_wstring(result.code);
        text(IDC_RESULT, message);
        manual.reset(); KillTimer(window, 1); enable(IDC_TEST, check >= 0);
    }
};
INT_PTR CALLBACK procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept {
    auto* self = reinterpret_cast<Dialog*>(GetWindowLongPtrW(window, DWLP_USER));
    try {
        if (message == WM_INITDIALOG) {
            self = reinterpret_cast<Dialog*>(lparam); self->window = window;
            SetWindowLongPtrW(window, DWLP_USER, reinterpret_cast<LONG_PTR>(self)); self->initialize(); return TRUE;
        }
        if (!self) return FALSE;
        if (message == WM_HELP) { self->help(); return TRUE; }
        if (message == WM_NOTIFY && reinterpret_cast<NMHDR*>(lparam)->idFrom == IDC_CHECKS && reinterpret_cast<NMHDR*>(lparam)->code == LVN_ITEMCHANGED) {
            self->select_check(*reinterpret_cast<NMLISTVIEW*>(lparam)); return TRUE;
        }
        if (message == WM_COMMAND) { self->command(LOWORD(wparam), HIWORD(wparam)); return TRUE; }
        if (message == WM_TIMER) { if (wparam == 2) self->refresh_status(); else if (wparam == 1) self->poll(); return TRUE; }
        if (message == WM_CLOSE) { EndDialog(window, IDCANCEL); return TRUE; }
        if (message == WM_DESTROY) { KillTimer(window, 1); KillTimer(window, 2); self->manual.reset(); return TRUE; }
    } catch (const std::exception& error) {
        if (self) {
            SendDlgItemMessageW(window, IDC_GROUPS, LB_SETCURSEL, self->group, 0);
            self->filling = true;
            ListView_SetItemState(GetDlgItem(window, IDC_CHECKS), -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
            if (self->check >= 0) ListView_SetItemState(GetDlgItem(window, IDC_CHECKS), self->check, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            self->filling = false;
            MessageBoxW(window, wide(error.what()).c_str(), L"NetWatch", MB_OK | MB_ICONERROR);
        }
    } catch (...) { MessageBoxW(window, self ? self->string(IDS_UNEXPECTED).c_str() : L"Unexpected error", L"NetWatch", MB_OK | MB_ICONERROR); }
    return FALSE;
}
}
bool show_settings(HWND parent, HINSTANCE module, Engine& engine, const std::filesystem::path& path,
    const std::wstring& error, const std::vector<Group>& exported, Language language) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES}; InitCommonControlsEx(&controls);
    Dialog dialog{module, engine, path, error, engine.configuration(), exported, language};
    const auto result = DialogBoxParamW(module, MAKEINTRESOURCEW(IDD_SETTINGS + resource_offset(language)), parent, procedure, reinterpret_cast<LPARAM>(&dialog));
    if (result == -1) throw std::runtime_error("Cannot create NetWatch settings dialog");
    return dialog.changed;
}
}
