// Compile the real dialog controller into an isolated hidden test process.
#include "../src/settings.cpp"
#include <iostream>

static void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
    try {
        nw::Engine engine(nw::initial_config());
        nw::Dialog dialog{GetModuleHandleW(nullptr), engine, {}, {}, engine.configuration()};
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
