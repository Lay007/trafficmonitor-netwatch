#pragma once
#include "engine.h"
#include <Windows.h>
#include <filesystem>
namespace nw {
bool show_settings(HWND parent, HINSTANCE module, Engine& engine, const std::filesystem::path& path,
    const std::wstring& load_error, const std::vector<Group>& exported, Language language);
}
