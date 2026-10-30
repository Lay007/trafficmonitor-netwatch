#include "settings.h"
#include "config.h"
#include "resource.h"
#include <commctrl.h>
#include <stdexcept>
#include <iomanip>
#include <sstream>

namespace nw {
namespace {
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
        wchar_t text[1024]{}; LoadStringW(module, id + (language == Language::Russian ? 1000 : 0), text, 1024); return text;
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
        SendDlgItemMessageW(window, IDC_CHECKS, LB_RESETCONTENT, 0, 0);
        if (group >= 0) {
            const auto& g = draft.groups.at(group);
            text(IDC_GROUP_NAME, g.name); text(IDC_GROUP_LABEL, g.label);
            checked(IDC_GROUP_ENABLED, g.enabled); checked(IDC_GROUP_VISIBLE, g.visible);
            for (const auto& c : g.checks) SendDlgItemMessageW(window, IDC_CHECKS, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(c.name.c_str()));
            check = g.checks.empty() ? -1 : (std::max)(0, (std::min)(check, static_cast<int>(g.checks.size()) - 1));
        } else {
            text(IDC_GROUP_NAME, L""); text(IDC_GROUP_LABEL, L""); checked(IDC_GROUP_ENABLED, false); checked(IDC_GROUP_VISIBLE, false); check = -1;
        }
        SendDlgItemMessageW(window, IDC_CHECKS, LB_SETCURSEL, check, 0);
        filling = false; fill_check();
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
        const auto all = string(IDS_ALL);
        for (const auto value : {all.c_str(), L"0", L"1", L"2", L"4", L"8"}) SendDlgItemMessageW(window, IDC_LIMIT, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
        for (const auto value : {L"ICMP", L"TCP", L"HTTP / HTTPS"}) SendDlgItemMessageW(window, IDC_CHECK_KIND, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
        text(IDC_LIMIT, draft.max_visible ? std::to_wstring(*draft.max_visible) : all);
        SendDlgItemMessageW(window, IDC_GROUP_NAME, EM_SETLIMITTEXT, 128, 0);
        SendDlgItemMessageW(window, IDC_GROUP_LABEL, EM_SETLIMITTEXT, 24, 0);
        SendDlgItemMessageW(window, IDC_CHECK_NAME, EM_SETLIMITTEXT, 128, 0);
        SendDlgItemMessageW(window, IDC_CHECK_TARGET, EM_SETLIMITTEXT, 2048, 0);
        fill_groups();
        if (!load_error.empty()) text(IDC_RESULT, load_error + L"\r\n" + string(IDS_LOAD_ERROR));
    }
    void command(int id, int notification) {
        if (filling) return;
        if (id == IDCANCEL) { EndDialog(window, IDCANCEL); return; }
        if (id == IDC_CHECK_KIND && notification == CBN_SELCHANGE) {
            enable(IDC_CHECK_PORT, SendDlgItemMessageW(window, IDC_CHECK_KIND, CB_GETCURSEL, 0, 0) == static_cast<int>(Kind::Tcp)); return;
        }
        if (id == IDC_GROUPS && notification == LBN_SELCHANGE) {
            const int next = static_cast<int>(SendDlgItemMessageW(window, IDC_GROUPS, LB_GETCURSEL, 0, 0));
            capture(); group = next; check = -1; fill_groups(); return;
        }
        if (id == IDC_CHECKS && notification == LBN_SELCHANGE) {
            const int next = static_cast<int>(SendDlgItemMessageW(window, IDC_CHECKS, LB_GETCURSEL, 0, 0));
            capture(); check = next; fill_group(); return;
        }
        if (notification != BN_CLICKED) return;
        if (id == IDC_GROUP_DELETE && group >= 0) {
            draft.groups.erase(draft.groups.begin() + group); check = -1; fill_groups(); return;
        }
        if (id == IDC_CHECK_DELETE && group >= 0 && check >= 0) {
            auto& checks = draft.groups.at(group).checks; checks.erase(checks.begin() + check); fill_group(); return;
        }
        if (id != IDOK && id != IDC_GROUP_ADD && id != IDC_GROUP_UP && id != IDC_GROUP_DOWN && id != IDC_CHECK_ADD && id != IDC_TEST) return;
        capture();
        if (id == IDC_GROUP_ADD) {
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
        message += result.has_measurement ? duration.str() + translated(language, L" ms", L" мс") : translated(language, L"n/a", L"нет данных");
        if (result.http_status) message += L"; HTTP " + std::to_wstring(result.http_status);
        if (result.code) message += translated(language, L"; code=", L"; код=") + std::to_wstring(result.code);
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
        if (message == WM_COMMAND) { self->command(LOWORD(wparam), HIWORD(wparam)); return TRUE; }
        if (message == WM_TIMER) { self->poll(); return TRUE; }
        if (message == WM_CLOSE) { EndDialog(window, IDCANCEL); return TRUE; }
        if (message == WM_DESTROY) { KillTimer(window, 1); self->manual.reset(); return TRUE; }
    } catch (const std::exception& error) {
        if (self) {
            SendDlgItemMessageW(window, IDC_GROUPS, LB_SETCURSEL, self->group, 0);
            SendDlgItemMessageW(window, IDC_CHECKS, LB_SETCURSEL, self->check, 0);
            MessageBoxW(window, wide(error.what()).c_str(), L"NetWatch", MB_OK | MB_ICONERROR);
        }
    } catch (...) { MessageBoxW(window, self ? self->string(IDS_UNEXPECTED).c_str() : L"Unexpected error", L"NetWatch", MB_OK | MB_ICONERROR); }
    return FALSE;
}
}
bool show_settings(HWND parent, HINSTANCE module, Engine& engine, const std::filesystem::path& path,
    const std::wstring& error, const std::vector<Group>& exported, Language language) {
    Dialog dialog{module, engine, path, error, engine.configuration(), exported, language};
    const auto result = DialogBoxParamW(module, MAKEINTRESOURCEW(language == Language::Russian ? IDD_SETTINGS_RU : IDD_SETTINGS), parent, procedure, reinterpret_cast<LPARAM>(&dialog));
    if (result == -1) throw std::runtime_error("Cannot create NetWatch settings dialog");
    return dialog.changed;
}
}
