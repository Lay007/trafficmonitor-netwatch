#include "config.h"
#include <Windows.h>
#include <psapi.h>
#ifdef NW_HOST_INTERFACE
#include NW_HOST_INTERFACE
#else
#include <PluginInterface.h>
#endif
#include <TlHelp32.h>
#include <atomic>
#include <iostream>
#include <thread>
#include <stdexcept>

static void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
using Factory = ITMPlugin* (*)();
class LanguageHost final : public ITrafficMonitor {
public:
    unsigned short language{0x419};
    int GetAPIVersion() override { return 0; }
    const wchar_t* GetVersion() override { return L"1.86"; }
    double GetMonitorValue(MonitorItem) override { return 0; }
    const wchar_t* GetMonitorValueString(MonitorItem, int) override { return L""; }
    void ShowNotifyMessage(const wchar_t*) override {}
    unsigned short GetLanguageId() const override { return language; }
    const wchar_t* GetPluginConfigDir() const override { return L""; }
    int GetDPI(DPIType) const override { return 96; }
    unsigned int GetThemeColor() const override { return 0; }
    const wchar_t* GetStringRes(const wchar_t*, const wchar_t*) override { return L""; }
};
static HMODULE load(const wchar_t* path, ITMPlugin*& plugin) {
    HMODULE module = LoadLibraryW(path); require(module != nullptr, "LoadLibrary");
    const auto factory = reinterpret_cast<Factory>(GetProcAddress(module, "TMPluginGetInstance"));
    require(factory != nullptr, "Undecorated TMPluginGetInstance export"); plugin = factory();
    require(plugin && plugin == factory() && plugin->GetAPIVersion() == 7, "Singleton and pinned API v7"); return module;
}
static void show_preview(const wchar_t* dll, const std::filesystem::path& directory, ITrafficMonitor* host) {
    ITMPlugin* plugin{}; const auto module = load(dll, plugin);
    plugin->OnExtenedInfo(ITMPlugin::EI_CONFIG_DIR, directory.c_str()); plugin->OnInitialize(host);
    std::atomic<bool> closed{false};
    std::thread scheduler([&] { while (!closed) { plugin->DataRequired(); Sleep(100); } });
    plugin->ShowOptionsDialog(nullptr);
    closed = true; scheduler.join(); FreeLibrary(module);
}
static unsigned thread_count() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 entry{}; entry.dwSize = sizeof(entry); unsigned count = 0;
    if (Thread32First(snapshot, &entry)) do { count += entry.th32OwnerProcessID == GetCurrentProcessId(); } while (Thread32Next(snapshot, &entry));
    CloseHandle(snapshot); return count;
}
static void tooltip_budget(const wchar_t* dll, const std::filesystem::path& directory) {
    auto config = nw::initial_config();
    config.groups[0].name = std::wstring(126, L'G') + L"\U0001F680";
    for (int i = 0; i < 9; ++i) {
        nw::Check check; check.id = nw::new_id(); check.kind = nw::Kind::Tcp;
        check.name = std::wstring(126, L'C') + L"\U0001F680";
        config.groups[0].checks.push_back(check);
    }
    nw::save_config(directory / L"NetWatch.conf", config);
    for (const auto& language : {std::pair<unsigned short, const wchar_t*>{0x409, L"More in NetWatch settings."},
        {0x419, L"Подробнее в настройках NetWatch."}, {0x804, L"更多信息请查看 NetWatch 设置。"}, {0x404, L"更多資訊請查看 NetWatch 設定。"}}) {
        LanguageHost host; host.language = language.first;
        ITMPlugin* plugin{}; const auto module = load(dll, plugin);
        plugin->OnExtenedInfo(ITMPlugin::EI_CONFIG_DIR, directory.c_str()); plugin->OnInitialize(&host);
        const std::wstring text = plugin->GetTooltipInfo();
        require(text.size() <= 512, "Plugin tooltip exceeds its share of the host's 1024-character limit");
        require(text.find(L"NetWatch") == 0 && text.find(language.second) != std::wstring::npos, "Truncated tooltip lacks localized notice");
        for (size_t i = 0; i < text.size(); ++i) {
            if (IS_HIGH_SURROGATE(text[i])) require(++i < text.size() && IS_LOW_SURROGATE(text[i]), "Tooltip split a UTF-16 pair");
            else require(!IS_LOW_SURROGATE(text[i]), "Tooltip contains an isolated low surrogate");
        }
        FreeLibrary(module);
    }
    std::cout << "PASS actual DLL bounded multilingual tooltips for large SSH-style configurations\n";
}
static void render_and_layout(const wchar_t* dll, const std::filesystem::path& directory) {
    nw::Config config;
    for (int i = 0; i < 4; ++i) {
        nw::Group group; group.id = nw::new_id(); group.name = L"Render " + std::to_wstring(i); group.label = group.name;
        if (i < 3) {
            nw::Check c; c.id = nw::new_id(); c.interval_ms = 1000; c.timeout_ms = 500;
            c.target = i == 2 ? L"bad host name" : L"127.0.0.1"; group.checks.push_back(c);
            if (i == 1) { c.id = nw::new_id(); c.target = L"bad host name"; group.checks.push_back(c); }
        }
        config.groups.push_back(group);
    }
    nw::save_config(directory / L"NetWatch.conf", config);
    ITMPlugin* plugin{}; auto module = load(dll, plugin);
    plugin->OnExtenedInfo(ITMPlugin::EI_CONFIG_DIR, directory.c_str()); plugin->OnInitialize(nullptr);
    const auto start = nw::Clock::now();
    while (nw::Clock::now() - start < std::chrono::seconds(4)) { plugin->DataRequired(); Sleep(50); }
    HDC dc = CreateCompatibleDC(nullptr);
    BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth = 160;
    info.bmiHeader.biHeight = -40; info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
    void* pixels{}; HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    require(bitmap != nullptr, "Create pixel inspection bitmap"); auto previous = SelectObject(dc, bitmap);
    const COLORREF expected[] = {RGB(40,185,85), RGB(235,175,30), RGB(230,65,65), RGB(145,150,155)};
    for (int i = 0; i < 4; ++i) {
        PatBlt(dc, 0, 0, 160, 40, BLACKNESS);
        auto item = plugin->GetItem(i); require(item != nullptr, "Four group items");
        item->DrawItem(dc, 0, 0, 160, 40, true);
        unsigned matches = 0;
        for (int y = 0; y < 40; ++y) for (int x = 100; x < 160; ++x) matches += GetPixel(dc, x, y) == expected[i];
        require(matches > 4, "Actual GDI circle color disagrees with network group status");
    }
    SelectObject(dc, previous); DeleteObject(bitmap); DeleteDC(dc);
    auto retained = plugin->GetItem(0); const wchar_t* retained_id = retained->GetItemId(); std::wstring identity = retained_id;
    config.max_visible = 0; nw::save_config(directory / L"NetWatch.conf", config);
    plugin->OnExtenedInfo(ITMPlugin::EI_CONFIG_DIR, directory.c_str()); plugin->OnInitialize(nullptr);
    require(plugin->GetItem(0) == retained && std::wstring(retained_id) == identity && plugin->GetItem(3), "Host item snapshot mutated before restart");
    FreeLibrary(module);
    for (int i = 0; i < 400 && GetModuleHandleW(L"NetWatch.dll"); ++i) Sleep(10);
    require(!GetModuleHandleW(L"NetWatch.dll"), "Render host workers did not drain");
    module = load(dll, plugin); plugin->OnExtenedInfo(ITMPlugin::EI_CONFIG_DIR, directory.c_str()); plugin->OnInitialize(nullptr);
    require(!plugin->GetItem(0), "Zero limit did not take effect on reload"); FreeLibrary(module);
    std::cout << "PASS actual DLL GDI green/yellow/red/gray pixels and restart-only item layout\n";
}
static unsigned long long cpu_ticks() {
    FILETIME created{}, exited{}, kernel{}, user{};
    GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
    ULARGE_INTEGER k{}, u{}; k.LowPart = kernel.dwLowDateTime; k.HighPart = kernel.dwHighDateTime;
    u.LowPart = user.dwLowDateTime; u.HighPart = user.dwHighDateTime; return k.QuadPart + u.QuadPart;
}
static void http_lifetime(const wchar_t* dll, const std::filesystem::path& directory, unsigned port) {
    const auto event_name = L"NetWatch-HTTP-" + nw::new_id();
    HANDLE request_started = CreateEventW(nullptr, FALSE, FALSE, event_name.c_str());
    require(request_started != nullptr, "Create HTTP synchronization event");
    auto config = nw::initial_config();
    config.groups.resize(1);
    config.groups[0].visible = false;
    for (int i = 0; i < 256; ++i) {
        nw::Check check; check.id = nw::new_id(); check.kind = nw::Kind::Http;
        check.target = L"http://127.0.0.1:" + std::to_wstring(port) + L"/hold/" + event_name;
        check.timeout_ms = 1500; config.groups[0].checks.push_back(check);
    }
    nw::save_config(directory / L"NetWatch.conf", config);
    const unsigned baseline_threads = thread_count(); DWORD baseline_handles{}; GetProcessHandleCount(GetCurrentProcess(), &baseline_handles);
    ITMPlugin* plugin{}; auto module = load(dll, plugin);
    plugin->OnExtenedInfo(ITMPlugin::EI_CONFIG_DIR, directory.c_str()); plugin->OnInitialize(nullptr);
    auto started = nw::Clock::now(); const auto cpu_start = cpu_ticks();
    unsigned max_threads = 0; DWORD max_handles = 0; SIZE_T max_memory = 0; unsigned sample = 0;
    while (nw::Clock::now() - started < std::chrono::seconds(8)) {
        plugin->DataRequired();
        if (sample++ % 10 == 0) max_threads = (std::max)(max_threads, thread_count());
        DWORD handles{}; GetProcessHandleCount(GetCurrentProcess(), &handles); max_handles = (std::max)(max_handles, handles);
        PROCESS_MEMORY_COUNTERS memory{}; memory.cb = sizeof(memory); GetProcessMemoryInfo(GetCurrentProcess(), &memory, sizeof(memory)); max_memory = (std::max)(max_memory, memory.WorkingSetSize);
        Sleep(100);
    }
    std::cout << "stress: 256 hidden HTTP checks; total process threads baseline=" << baseline_threads << " max=" << max_threads << "; handles baseline=" << baseline_handles << " max=" << max_handles << "; max working set=" << max_memory << "; CPU ms=" << (cpu_ticks() - cpu_start) / 10000.0 << " over 8 seconds\n";
    ResetEvent(request_started);
    bool observed_request = false;
    for (int i = 0; i < 50; ++i) {
        plugin->DataRequired();
        if (WaitForSingleObject(request_started, 100) == WAIT_OBJECT_0) { observed_request = true; break; }
    }
    require(observed_request, "Server did not confirm active HTTP request");
    // Existing workers retain their module references while WinHTTP cancels/drains.
    require(GetModuleHandleW(L"NetWatch.dll") != nullptr, "DLL missing before host release");
    FreeLibrary(module);
    require(GetModuleHandleW(L"NetWatch.dll") != nullptr, "No worker retained the module during active HTTP");
    for (int i = 0; i < 600 && GetModuleHandleW(L"NetWatch.dll"); ++i) Sleep(10);
    require(!GetModuleHandleW(L"NetWatch.dll"), "HTTP callback drain did not release the DLL");
    CloseHandle(request_started);
    std::cout << "PASS actual DLL release during active HTTP with 256 queued checks\n";
}
int wmain(int argc, wchar_t** argv) {
    try {
        if (argc == 1) {
            wchar_t executable[32768]{}; GetModuleFileNameW(nullptr, executable, 32768);
            auto directory = std::filesystem::path(executable).parent_path();
            show_preview((directory / L"NetWatch.dll").c_str(), directory / L"preview-config", nullptr); return 0;
        }
        require(argc >= 3, "Usage: netwatch_host DLL config-directory [--ui|--ui-zh-cn|--ui-zh-tw]");
        const std::filesystem::path directory = std::filesystem::absolute(argv[2]);
        std::filesystem::create_directories(directory);
        if (argc == 4 && std::wstring(argv[3]) == L"--tooltip") { tooltip_budget(argv[1], directory); return 0; }
        if (argc == 5 && std::wstring(argv[3]) == L"--http") {
            http_lifetime(argv[1], directory, static_cast<unsigned>(std::stoul(argv[4]))); return 0;
        }
        if (argc == 4 && std::wstring(argv[3]) == L"--render") { render_and_layout(argv[1], directory); return 0; }
        if (argc == 4 && (std::wstring(argv[3]) == L"--ui" || std::wstring(argv[3]) == L"--ui-zh-cn" || std::wstring(argv[3]) == L"--ui-zh-tw")) {
            LanguageHost host; host.language = std::wstring(argv[3]) == L"--ui-zh-cn" ? 0x804 : 0x404;
            show_preview(argv[1], directory, std::wstring(argv[3]) == L"--ui" ? nullptr : &host); return 0;
        }
        nw::Config config = nw::initial_config();
        config.groups.resize(1);
        auto c = nw::Check{}; c.id = nw::new_id(); c.target = L"192.0.2.1"; c.timeout_ms = 500;
        config.groups[0].checks.push_back(c);
        const auto config_path = directory / L"NetWatch.conf"; nw::save_config(config_path, config);
        for (const auto& entry : {std::pair<unsigned short, const wchar_t*>{0x419, L"Локальные устройства"}, {0x804, L"本地设备"}, {0x404, L"本機裝置"}}) {
            LanguageHost language_host; language_host.language = entry.first;
            ITMPlugin* fresh{}; auto fresh_module = load(argv[1], fresh);
            const auto fresh_dir = directory / nw::new_id();
            fresh->OnExtenedInfo(ITMPlugin::EI_CONFIG_DIR, fresh_dir.c_str()); fresh->OnInitialize(&language_host);
            require(fresh->GetItem(2) && !fresh->GetItem(3), "First launch must export three default groups");
            require(std::wstring(fresh->GetItem(0)->GetItemName()) == entry.second && std::wstring(fresh->GetItem(2)->GetItemName()) == L"SSH", "Fresh groups do not follow host language");
            require(!std::filesystem::exists(fresh_dir), "First launch wrote configuration automatically");
            FreeLibrary(fresh_module);
        }
        struct LanguageCase { unsigned short id; const wchar_t* untested; const wchar_t* description; };
        for (const auto& language : {LanguageCase{0x419, L"ещё не проверено", L"Группы"}, LanguageCase{0x409, L"not tested", L"Network"}, LanguageCase{0x804, L"尚未检测", L"网络"}, LanguageCase{0x404, L"尚未檢查", L"網路"}}) {
            LanguageHost host; host.language = language.id;
            ITMPlugin* localized{}; auto localized_module = load(argv[1], localized);
            localized->OnExtenedInfo(ITMPlugin::EI_CONFIG_DIR, directory.c_str()); localized->OnInitialize(&host);
            require(std::wstring(localized->GetTooltipInfo()).find(language.untested) != std::wstring::npos, "Actual DLL ignores host language");
            require(std::wstring(localized->GetInfo(ITMPlugin::TMI_DESCRIPTION)).find(language.description) != std::wstring::npos, "Localized plugin description");
            require(std::wstring(localized->GetItem(0)->GetItemName()) == config.groups[0].name, "Language switch renamed configured group");
            FreeLibrary(localized_module);
        }
        ITMPlugin* plugin{}; HMODULE module = load(argv[1], plugin);
        plugin->OnExtenedInfo(ITMPlugin::EI_CONFIG_DIR, directory.c_str()); plugin->OnInitialize(nullptr);
        auto item = plugin->GetItem(0);
        require(item && !plugin->GetItem(-1) && !plugin->GetItem(1), "Enumeration boundary");
        require(std::wstring(item->GetItemId()) == L"netwatch." + config.groups[0].id, "Stable item identity");
        require(item->IsCustomDraw() && item->GetItemWidth() > 0, "Custom draw slots");
        require(item->IsDrawResourceUsageGraph() == 0 && item->GetResourceUsageGraphValue() == 0, "Last API v7 item slots");
        HDC dc = CreateCompatibleDC(nullptr); HBITMAP bitmap = CreateBitmap(300, 40, 1, 32, nullptr);
        auto previous = SelectObject(dc, bitmap); require(item->GetItemWidthEx(dc) > 0, "Font measurement");
        for (int i = 0; i < 100; ++i) item->DrawItem(dc, 0, 0, 120, 30, true);
        SelectObject(dc, previous); DeleteObject(bitmap); DeleteDC(dc);
        std::wstring name = item->GetItemName(); const wchar_t* name_ptr = item->GetItemName();
        const wchar_t* tooltip = plugin->GetTooltipInfo(); std::wstring before = tooltip;
        std::vector<std::thread> readers;
        for (int i = 0; i < 8; ++i) readers.emplace_back([&] { for (int j = 0; j < 100; ++j) { plugin->GetTooltipInfo(); item->GetItemName(); } });
        for (auto& thread : readers) thread.join();
        require(std::wstring(tooltip) == before && std::wstring(name_ptr) == name, "String pointer lifetime across other threads");
        plugin->DataRequired(); FreeLibrary(module);
        // A worker may complete immediately on some networks; actual unloading must still be safe.
        for (int i = 0; i < 500 && GetModuleHandleW(L"NetWatch.dll"); ++i) Sleep(10);
        require(!GetModuleHandleW(L"NetWatch.dll"), "DLL failed to unload after active probe");
        DWORD initial_handles{}; GetProcessHandleCount(GetCurrentProcess(), &initial_handles);
        const DWORD initial_gdi = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
        PROCESS_MEMORY_COUNTERS memory{}; memory.cb = sizeof(memory); GetProcessMemoryInfo(GetCurrentProcess(), &memory, sizeof(memory));
        const auto initial_memory = memory.WorkingSetSize;
        for (int i = 0; i < 100; ++i) { module = load(argv[1], plugin); plugin->OnExtenedInfo(ITMPlugin::EI_CONFIG_DIR, directory.c_str()); plugin->OnInitialize(nullptr); FreeLibrary(module); }
        DWORD final_handles{}; GetProcessHandleCount(GetCurrentProcess(), &final_handles);
        require(final_handles <= initial_handles + 2, "Handle growth across load/unload");
        require(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) == initial_gdi, "GDI handle growth");
        GetProcessMemoryInfo(GetCurrentProcess(), &memory, sizeof(memory));
        std::cout << "PASS DLL ABI v7, export, GDI, concurrent getters, active unload, 100 reloads\nhandles=" << initial_handles << "->" << final_handles << "; working set=" << initial_memory << "->" << memory.WorkingSetSize << " bytes\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << "FAIL host: " << error.what() << '\n'; return 1; }
}
