#include "engine.h"
#include "config.h"
#include "settings.h"
#include <Windows.h>
#include <PluginInterface.h>
#include <memory>
#include <mutex>

namespace nw {
class Item final : public IPluginItem {
    Engine& engine;
    const std::wstring group_id, id, name, label;
public:
    Item(Engine& e, const Group& g) : engine(e), group_id(g.id), id(L"netwatch." + g.id), name(g.name), label(g.label) {}
    const wchar_t* GetItemName() const override { return name.c_str(); }
    const wchar_t* GetItemId() const override { return id.c_str(); }
    const wchar_t* GetItemLableText() const override { return L""; }
    const wchar_t* GetItemValueText() const override { return L""; }
    const wchar_t* GetItemValueSampleText() const override { return label.c_str(); }
    bool IsCustomDraw() const override { return true; }
    int GetItemWidth() const override { return static_cast<int>(label.size()) * 10 + 20; }
    int GetItemWidthEx(void* context) const override {
        HDC dc = static_cast<HDC>(context); SIZE text{}; TEXTMETRICW metrics{};
        GetTextExtentPoint32W(dc, label.c_str(), static_cast<int>(label.size()), &text);
        GetTextMetricsW(dc, &metrics);
        return text.cx + (std::max)(10L, metrics.tmHeight * 3 / 4) + (std::max)(4L, metrics.tmHeight / 3);
    }
    void DrawItem(void* context, int x, int y, int w, int h, bool) override {
        HDC dc = static_cast<HDC>(context);
        const int saved = SaveDC(dc);
        if (!saved) return;
        IntersectClipRect(dc, x, y, x + w, y + h);
        TEXTMETRICW metrics{}; GetTextMetricsW(dc, &metrics);
        const int diameter = (std::max)(4, (std::min)(h - 4, static_cast<int>(metrics.tmHeight * 3 / 5)));
        RECT text{x, y, x + w - diameter - 5, y + h};
        SetBkMode(dc, TRANSPARENT);
        DrawTextW(dc, label.c_str(), static_cast<int>(label.size()), &text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        COLORREF color = RGB(145, 150, 155);
        switch (engine.group_health(group_id)) {
        case Health::Up: color = RGB(40, 185, 85); break;
        case Health::Mixed: color = RGB(235, 175, 30); break;
        case Health::Down: color = RGB(230, 65, 65); break;
        default: break;
        }
        HBRUSH brush = CreateSolidBrush(color);
        if (brush) {
            SelectObject(dc, brush); SelectObject(dc, GetStockObject(NULL_PEN));
            const int left = x + w - diameter - 1, top = y + (h - diameter) / 2;
            Ellipse(dc, left, top, left + diameter, top + diameter);
        }
        RestoreDC(dc, saved);
        if (brush) DeleteObject(brush);
    }
};
class Plugin final : public ITMPlugin {
    Engine engine{initial_config()};
    std::filesystem::path path;
    std::wstring load_error;
    std::vector<std::unique_ptr<Item>> items;
    bool initialized{};
    std::mutex tooltip_mutex;
    std::map<DWORD, std::wstring> tooltips;
public:
    IPluginItem* GetItem(int index) override {
        return index >= 0 && static_cast<size_t>(index) < items.size() ? items[index].get() : nullptr;
    }
    void DataRequired() override { try { if (initialized) engine.dispatch(); } catch (...) {} }
    const wchar_t* GetInfo(PluginInfoIndex index) override {
        switch (index) {
        case TMI_NAME: return L"NetWatch";
        case TMI_DESCRIPTION: return L"Network availability groups (ICMP, TCP, HTTP/HTTPS)";
        case TMI_AUTHOR: return L"NetWatch contributors";
        case TMI_COPYRIGHT: return L"NetWatch contributors, 2026";
        case TMI_VERSION: return L"0.1.0";
        default: return L"";
        }
    }
    void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* value) override {
        if (index != EI_CONFIG_DIR || initialized || !value) return;
        path = std::filesystem::path(value) / L"NetWatch.conf";
        try { engine.apply(load_config(path)); }
        catch (const std::exception& error) { load_error = L"Configuration could not be loaded: " + wide(error.what()) + L". The original file has not been changed."; }
    }
    void OnInitialize(ITrafficMonitor*) override {
        if (initialized) return;
        for (const auto& group : visible_groups(engine.configuration())) items.push_back(std::make_unique<Item>(engine, group));
        initialized = true;
    }
    OptionReturn ShowOptionsDialog(void* parent) override {
        if (path.empty()) return OR_OPTION_NOT_PROVIDED;
        HMODULE module{};
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&TMPluginAnchor), &module);
        try {
            if (show_settings(static_cast<HWND>(parent), module, engine, path, load_error)) {
                std::lock_guard<std::mutex> lock(tooltip_mutex); load_error.clear(); return OR_OPTION_CHANGED;
            }
            return OR_OPTION_UNCHANGED;
        }
        catch (const std::exception& error) { MessageBoxW(static_cast<HWND>(parent), wide(error.what()).c_str(), L"NetWatch", MB_OK | MB_ICONERROR); return OR_OPTION_UNCHANGED; }
    }
    const wchar_t* GetTooltipInfo() override {
        auto text = engine.tooltip();
        std::lock_guard<std::mutex> lock(tooltip_mutex);
        if (!load_error.empty()) text += L"\r\n" + load_error;
        auto& storage = tooltips[GetCurrentThreadId()]; storage = std::move(text); return storage.c_str();
    }
    static void TMPluginAnchor() {}
};
}
extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance() {
    try { static nw::Plugin instance; return &instance; } catch (...) { return nullptr; }
}
