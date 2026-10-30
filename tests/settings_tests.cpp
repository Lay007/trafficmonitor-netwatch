// Compile the real dialog controller into an isolated hidden test process.
#include <Windows.h>
#include <string>
#include <vector>
static std::vector<std::wstring> notifications;
static int WINAPI record_message_box(HWND, LPCWSTR text, LPCWSTR, UINT) {
    notifications.emplace_back(text); return IDOK;
}
#define MessageBoxW record_message_box
#include "../src/settings.cpp"
#undef MessageBoxW
#include <iostream>

static void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
    try {
        {
            nw::Engine live(nw::initial_config());
            const auto folder = std::filesystem::temp_directory_path() / nw::new_id();
            const auto exported = nw::visible_groups(live.configuration());
            nw::Dialog save{GetModuleHandleW(nullptr), live, folder / L"NetWatch.conf", {}, live.configuration(), exported};
            HWND save_window = CreateDialogParamW(save.module, MAKEINTRESOURCEW(IDD_SETTINGS), nullptr, nw::procedure, reinterpret_cast<LPARAM>(&save));
            require(save_window != nullptr, "Create save regression dialog");
            require(save.text(IDOK) == L"Save" && save.text(IDC_LIMIT) == L"All", "Explicit English resource selection");
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
        nw::Engine engine(nw::initial_config());
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
        std::cout << "PASS real settings controller: group/check editing, ordering, IDs, limit, validation, draft tests and close during probe\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL settings: " << e.what() << '\n'; return 1; }
}
