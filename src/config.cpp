#include "config.h"
#include <Windows.h>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace nw {
Config load_config(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return initial_config();
    if (std::filesystem::file_size(path) > 16 * 1024 * 1024) throw std::runtime_error("Configuration exceeds 16 MiB");
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open configuration");
    Config config;
    std::string line;
    if (!std::getline(file, line)) throw std::runtime_error("Missing configuration version");
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.rfind("\xEF\xBB\xBF", 0) == 0) line.erase(0, 3);
    if (line != "NetWatch 1") throw std::runtime_error("Unknown configuration version");
    bool has_limit = false;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::istringstream s(line); s.imbue(std::locale::classic());
        std::string tag; s >> tag;
        if (tag == "max_visible") {
            if (has_limit) throw std::runtime_error("Duplicate display limit");
            std::string value; s >> value;
            if (value != "All") {
                if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos) throw std::runtime_error("Invalid display limit");
                const auto n = std::stoull(value);
                if (n > 4096) throw std::runtime_error("Display limit exceeds 4096");
                config.max_visible = static_cast<unsigned>(n);
            }
            has_limit = true;
        } else if (tag == "group") {
            std::string id, name, label; Group g;
            s >> std::quoted(id) >> std::quoted(name) >> std::quoted(label) >> g.enabled >> g.visible;
            g.id = wide(id); g.name = wide(name); g.label = wide(label);
            config.groups.push_back(std::move(g));
        } else if (tag == "check" && !config.groups.empty()) {
            Check c; std::string id, name, target; int kind = -1;
            s >> std::quoted(id) >> std::quoted(name) >> kind >> std::quoted(target) >> c.port >> c.interval_ms >> c.timeout_ms >> c.enabled;
            c.id = wide(id); c.name = wide(name); c.target = wide(target); c.kind = static_cast<Kind>(kind);
            config.groups.back().checks.push_back(std::move(c));
        } else throw std::runtime_error("Unknown or misplaced configuration record");
        if (s.fail()) throw std::runtime_error("Malformed configuration record");
        s >> std::ws;
        if (!s.eof()) throw std::runtime_error("Trailing configuration data");
    }
    if (!file.eof()) throw std::runtime_error("Configuration read failed");
    if (!has_limit) throw std::runtime_error("Missing display limit");
    auto error = validate(config);
    if (!error.empty()) throw std::runtime_error(utf8(error));
    return config;
}
void save_config(const std::filesystem::path& path, const Config& config) {
    auto error = validate(config);
    if (!error.empty()) throw std::runtime_error(utf8(error));
    std::ostringstream s; s.imbue(std::locale::classic());
    s << "NetWatch 1\nmax_visible " << (config.max_visible ? std::to_string(*config.max_visible) : "All") << '\n';
    for (const auto& g : config.groups) {
        s << "group " << std::quoted(utf8(g.id)) << ' ' << std::quoted(utf8(g.name)) << ' ' << std::quoted(utf8(g.label)) << ' ' << g.enabled << ' ' << g.visible << '\n';
        for (const auto& c : g.checks)
            s << "check " << std::quoted(utf8(c.id)) << ' ' << std::quoted(utf8(c.name)) << ' ' << static_cast<int>(c.kind) << ' ' << std::quoted(utf8(c.target)) << ' ' << c.port << ' ' << c.interval_ms << ' ' << c.timeout_ms << ' ' << c.enabled << '\n';
    }
    std::filesystem::create_directories(path.parent_path());
    const auto temp = path.wstring() + L"." + new_id() + L".tmp";
    try {
        std::ofstream file(temp, std::ios::binary | std::ios::trunc);
        const auto bytes = s.str(); file.write(bytes.data(), static_cast<std::streamsize>(bytes.size())); file.flush();
        if (!file) throw std::runtime_error("Cannot write configuration");
        file.close();
        if (!file || !MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("Cannot replace configuration");
    } catch (...) { DeleteFileW(temp.c_str()); throw; }
}
}
