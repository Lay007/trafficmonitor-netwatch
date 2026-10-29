#pragma once
#include <atlimage.h>
#include <fstream>

// Included only by the disposable, instrumented upstream test host.
// Uses that host's live taskbar object and unmodified ShowInfo renderer.
inline void NetWatchIntegrationTick(CTrafficMonitorDlg& main_window) {
    static const ULONGLONG started = GetTickCount64();
    static bool captured = false;
    const std::wstring request = theApp.m_module_dir + L"NetWatch.capture";
    if (captured || GetTickCount64() - started < 8000 || GetFileAttributesW(request.c_str()) == INVALID_FILE_ATTRIBUTES) return;
    auto* taskbar = main_window.GetTaskbarWindow();
    if (!taskbar || !IsWindow(taskbar->GetSafeHwnd())) return;
    CRect rect; taskbar->GetClientRect(&rect);
    if (rect.Width() <= 0 || rect.Height() <= 0) return;
    captured = true;
    const auto write_utf8 = [](const std::wstring& path, const std::wstring& text) {
        const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        std::string bytes(length, '\0');
        if (length) WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), bytes.data(), length, nullptr, nullptr);
        std::ofstream file(path, std::ios::binary); file.write(bytes.data(), bytes.size());
    };
    CImage image; image.Create(rect.Width(), rect.Height(), 32);
    CDC target; target.Attach(image.GetDC());
    taskbar->ShowInfo(&target);
    target.Detach(); image.ReleaseDC();
    const HRESULT saved = image.Save((theApp.m_module_dir + L"NetWatch-taskbar.png").c_str());
    CString tooltip; taskbar->m_tool_tips.GetText(tooltip, taskbar);
    write_utf8(theApp.m_module_dir + L"NetWatch-tooltip.txt", tooltip.GetString());
    std::wstring report = L"width=" + std::to_wstring(rect.Width()) + L"\nheight=" + std::to_wstring(rect.Height()) +
        L"\ndpi=" + std::to_wstring(taskbar->GetDPI()) + L"\nvisible=" + std::to_wstring(taskbar->IsWindowVisible() != FALSE) +
        L"\ninsert_failed=" + std::to_wstring(taskbar->GetCannotInsertToTaskBar()) + L"\nimage_hresult=" + std::to_wstring(saved) + L"\n";
    for (const auto* item : theApp.m_plugins.GetPluginItems()) report += std::wstring(L"item=") + item->GetItemId() + L"\n";
    write_utf8(theApp.m_module_dir + L"NetWatch-integration.txt", report);
}
