#pragma once
#include "model.h"
#include <filesystem>
namespace nw {
Config load_config(const std::filesystem::path& path);
void save_config(const std::filesystem::path& path, const Config& config);
}
