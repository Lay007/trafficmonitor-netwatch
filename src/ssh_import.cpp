#include "ssh_import.h"
#include <Windows.h>
#include <algorithm>
#include <cwctype>
#include <fstream>
#include <set>
#include <stdexcept>

namespace nw {
namespace {
std::wstring lower(std::wstring text) {
    for (auto& c : text) c = static_cast<wchar_t>(std::towlower(c));
    return text;
}
bool pattern(const std::wstring& value, const std::wstring& mask) {
    size_t v = 0, m = 0, star = std::wstring::npos, retry = 0;
    while (v < value.size()) {
        if (m < mask.size() && (mask[m] == L'?' || mask[m] == value[v])) { ++v; ++m; }
        else if (m < mask.size() && mask[m] == L'*') { star = m++; retry = v; }
        else if (star != std::wstring::npos) { m = star + 1; v = ++retry; }
        else return false;
    }
    while (m < mask.size() && mask[m] == L'*') ++m;
    return m == mask.size();
}
bool matches(const std::wstring& alias, const std::vector<std::wstring>& masks) {
    bool positive = false;
    for (const auto& mask : masks) {
        const bool negated = !mask.empty() && mask[0] == L'!';
        if (pattern(alias, negated ? mask.substr(1) : mask)) {
            if (negated) return false;
            positive = true;
        }
    }
    return positive;
}
std::vector<std::wstring> tokens(const std::wstring& line) {
    std::vector<std::wstring> result;
    std::wstring token; wchar_t quote = 0; bool started = false;
    for (size_t i = 0; i < line.size(); ++i) {
        const auto c = line[i];
        if (!quote && c == L'#') break;
        if (c == L'\\' && i + 1 < line.size() && (line[i + 1] == quote || line[i + 1] == L'\\' || (!quote && std::iswspace(line[i + 1])))) {
            token += line[++i]; started = true;
        } else if (quote && c == quote) quote = 0;
        else if (!quote && (c == L'"' || c == L'\'')) { quote = c; started = true; }
        else if (!quote && std::iswspace(c)) {
            if (started) { result.push_back(std::move(token)); token.clear(); started = false; }
        } else { token += c; started = true; }
    }
    if (quote) throw std::runtime_error("Unclosed quote in OpenSSH config");
    if (started) result.push_back(std::move(token));
    return result;
}
struct Node {
    std::wstring key;
    std::vector<std::wstring> args;
    std::vector<std::vector<Node>> included;
};
struct Reader {
    std::filesystem::path home;
    size_t files{}, bytes{}, records{};
    std::set<std::wstring> stack, aliases_seen;
    std::vector<std::wstring> aliases;
    std::filesystem::path include_path(std::wstring value) const {
        if (value.rfind(L"~/", 0) == 0 || value.rfind(L"~\\", 0) == 0) value = home.wstring() + value.substr(1);
        if (value.find_first_of(L"~%$") != std::wstring::npos) throw std::runtime_error("Unsupported expansion in OpenSSH Include path");
        std::filesystem::path result(value);
        return result.is_absolute() ? result : home / L".ssh" / result;
    }
    std::vector<Node> read(const std::filesystem::path& path, unsigned depth = 0) {
        const auto full = std::filesystem::weakly_canonical(std::filesystem::absolute(path));
        const auto identity = lower(full.wstring());
        if (identity.rfind(L"\\\\", 0) == 0 || GetDriveTypeW(full.root_path().c_str()) == DRIVE_REMOTE)
            throw std::runtime_error("OpenSSH import requires local configuration files");
        if (depth > 8 || ++files > 64 || !stack.insert(identity).second) throw std::runtime_error("OpenSSH Include cycle or file/depth limit exceeded");
        struct Pop { std::set<std::wstring>& stack; std::wstring key; ~Pop() { stack.erase(key); } } pop{stack, identity};
        std::ifstream file(full, std::ios::binary);
        if (!file) throw std::runtime_error("Cannot open OpenSSH config or Include file");
        if (std::filesystem::file_size(full) > 16 * 1024 * 1024 - bytes) throw std::runtime_error("OpenSSH config exceeds import byte limit");
        std::vector<Node> nodes;
        std::string line;
        bool first = true;
        while (std::getline(file, line)) {
            bytes += line.size() + 1;
            if (bytes > 16 * 1024 * 1024 || line.size() > 16384 || ++records > 65536) throw std::runtime_error("OpenSSH config exceeds import limits");
            if (first && line.rfind("\xEF\xBB\xBF", 0) == 0) line.erase(0, 3);
            first = false;
            auto text = wide(line);
            // Only the keyword/value separator treats '=' specially.
            const auto start = text.find_first_not_of(L" \t\r");
            if (start == std::wstring::npos || text[start] == L'#') continue;
            const auto end = text.find_first_of(L" \t=\r", start);
            Node node; node.key = lower(text.substr(start, end == std::wstring::npos ? end : end - start));
            if (end != std::wstring::npos) {
                auto value = text.find_first_not_of(L" \t\r", end);
                if (value != std::wstring::npos && text[value] == L'=') value = text.find_first_not_of(L" \t\r", value + 1);
                if (value != std::wstring::npos) node.args = tokens(text.substr(value));
            }
            if (node.key == L"match") throw std::runtime_error("OpenSSH Match is not supported; no commands are executed. Import a config containing Host sections only.");
            if (node.args.empty()) throw std::runtime_error("Missing value in OpenSSH config");
            if (node.key == L"host") {
                for (const auto& alias : node.args) {
                    if (alias.empty()) throw std::runtime_error("Empty OpenSSH Host pattern");
                    if (alias[0] != L'!' && alias.find_first_of(L"*?") == std::wstring::npos && aliases_seen.insert(alias).second) {
                        if (aliases.size() >= 16384) throw std::runtime_error("Too many OpenSSH Host aliases");
                        aliases.push_back(alias);
                    }
                }
            } else if (node.key == L"include") {
                for (const auto& argument : node.args) {
                    const auto candidate = include_path(argument);
                    std::vector<std::filesystem::path> paths;
                    if (candidate.parent_path().wstring().find_first_of(L"*?") != std::wstring::npos)
                        throw std::runtime_error("OpenSSH Include wildcards are supported in file names only");
                    if (candidate.filename().wstring().find_first_of(L"*?") == std::wstring::npos) paths.push_back(candidate);
                    else {
                        if (!std::filesystem::exists(candidate.parent_path())) continue;
                        for (const auto& entry : std::filesystem::directory_iterator(candidate.parent_path())) {
                            if (entry.is_regular_file() && pattern(entry.path().filename().wstring(), candidate.filename().wstring())) paths.push_back(entry.path());
                            if (paths.size() > 64) throw std::runtime_error("Too many OpenSSH Include files");
                        }
                        std::sort(paths.begin(), paths.end());
                    }
                    for (const auto& child : paths) {
                        node.included.push_back(read(child, depth + 1));
                    }
                }
            }
            nodes.push_back(std::move(node));
        }
        if (!file.eof()) throw std::runtime_error("Cannot read OpenSSH config");
        return nodes;
    }
};
using Options = std::map<std::wstring, std::vector<std::wstring>>;
void evaluate(const std::vector<Node>& nodes, const std::wstring& alias, bool& active, bool& declared, Options& options, size_t& work) {
    for (const auto& node : nodes) {
        if (++work > 4000000) throw std::runtime_error("OpenSSH config exceeds evaluation complexity limit");
        if (node.key == L"host") {
            active = matches(alias, node.args);
            if (active && std::find(node.args.begin(), node.args.end(), alias) != node.args.end()) declared = true;
        } else if (node.key == L"include") {
            // Each included file inherits, but must not replace, its parent's Host state.
            if (active) for (const auto& file : node.included) {
                bool included_active = active; evaluate(file, alias, included_active, declared, options, work);
            }
        } else if (active && (node.key == L"hostname" || node.key == L"port" || node.key == L"proxyjump" || node.key == L"proxycommand" || node.key == L"canonicalizehostname")) {
            options.emplace(node.key, node.args);
        }
    }
}
bool valid_host(const std::wstring& value) {
    return !value.empty() && value.size() <= 2048 && value.find_first_of(L" \t\r\n\"'\\/[]@*?") == std::wstring::npos &&
        std::none_of(value.begin(), value.end(), [](wchar_t c) { return c < 32 || c == 127; });
}
}
std::vector<SshHost> read_ssh_config(const std::filesystem::path& path, const std::filesystem::path& home) {
    Reader reader{home, 0, 0, 0, {}, {}, {}};
    const auto nodes = reader.read(path);
    std::vector<SshHost> hosts;
    size_t work = 0;
    for (const auto& alias : reader.aliases) {
        SshHost host; host.alias = alias; host.target = alias;
        Options options; bool active = true, declared = false;
        evaluate(nodes, alias, active, declared, options, work);
        auto value = [&](const wchar_t* key, const wchar_t* fallback) {
            const auto found = options.find(key);
            return found == options.end() ? std::wstring(fallback) : found->second.size() == 1 ? found->second[0] : std::wstring();
        };
        if (!declared) host.issue = SshIssue::Inactive;
        else if (lower(value(L"proxyjump", L"none")) != L"none" || lower(value(L"proxycommand", L"none")) != L"none") host.issue = SshIssue::Proxy;
        else if (lower(value(L"canonicalizehostname", L"no")) != L"no") host.issue = SshIssue::Canonicalization;
        else {
            const auto target = value(L"hostname", alias.c_str()); host.target.clear();
            for (size_t i = 0; i < target.size(); ++i) {
                if (target[i] != L'%') host.target += target[i];
                else if (++i < target.size() && target[i] == L'h') host.target += alias;
                else if (i < target.size() && target[i] == L'%') host.target += L'%';
                else { host.issue = SshIssue::Tokens; break; }
            }
            const auto port = value(L"port", L"22");
            if (target.find(L"${") != std::wstring::npos) host.issue = SshIssue::Tokens;
            if (host.issue == SshIssue::None && (!valid_host(alias) || alias.size() > 128 || !valid_host(host.target))) host.issue = SshIssue::InvalidHost;
            if (host.issue == SshIssue::None) {
                if (port.empty() || port.size() > 5 || port.find_first_not_of(L"0123456789") != std::wstring::npos) host.issue = SshIssue::InvalidPort;
                else { host.port = static_cast<unsigned>(std::stoul(port)); if (!host.port || host.port > 65535) host.issue = SshIssue::InvalidPort; }
            }
        }
        hosts.push_back(std::move(host));
    }
    return hosts;
}
const wchar_t* ssh_issue_text(SshIssue issue, Language language) {
    switch (issue) {
    case SshIssue::None: return translated(language, L"Ready (TCP)", L"Готово (TCP)", L"就绪（TCP）", L"就緒（TCP）");
    case SshIssue::Proxy: return translated(language, L"Skipped: proxy/jump host", L"Пропуск: прокси/промежуточный узел", L"已跳过：代理/跳板机", L"已略過：代理/跳板主機");
    case SshIssue::Canonicalization: return translated(language, L"Skipped: hostname canonicalization", L"Пропуск: преобразование имени SSH", L"已跳过：主机名规范化", L"已略過：主機名稱正規化");
    case SshIssue::InvalidHost: return translated(language, L"Skipped: invalid host/name", L"Пропуск: неверный узел/название", L"已跳过：无效主机/名称", L"已略過：無效主機/名稱");
    case SshIssue::InvalidPort: return translated(language, L"Skipped: invalid port", L"Пропуск: неверный порт", L"已跳过：无效端口", L"已略過：無效連接埠");
    case SshIssue::Tokens: return translated(language, L"Skipped: unsupported tokens", L"Пропуск: неподдерживаемые подстановки", L"已跳过：不支持的变量", L"已略過：不支援的變數");
    case SshIssue::Inactive: return translated(language, L"Skipped: conditional Include", L"Пропуск: условный Include", L"已跳过：Include 条件未满足", L"已略過：Include 條件未滿足");
    default: return translated(language, L"Already in this group", L"Уже есть в этой группе", L"该组中已存在", L"此群組中已存在");
    }
}
std::vector<SshIssue> ssh_import_status(const Group& group, const std::vector<SshHost>& hosts) {
    std::set<std::wstring> names;
    std::set<std::pair<std::wstring, unsigned>> endpoints;
    for (const auto& check : group.checks) if (check.kind == Kind::Tcp) { names.insert(check.name); endpoints.emplace(lower(check.target), check.port); }
    std::vector<SshIssue> issues;
    for (const auto& host : hosts) {
        auto issue = host.issue;
        if (issue == SshIssue::None) {
            if (names.count(host.alias) || endpoints.count({lower(host.target), host.port})) issue = SshIssue::Duplicate;
            else { names.insert(host.alias); endpoints.emplace(lower(host.target), host.port); }
        }
        issues.push_back(issue);
    }
    return issues;
}
size_t append_ssh_hosts(Config& config, const std::wstring& group_id, const std::vector<SshHost>& hosts) {
    auto next = config;
    auto group = std::find_if(next.groups.begin(), next.groups.end(), [&](const Group& g) { return g.id == group_id; });
    if (group == next.groups.end()) throw std::runtime_error("SSH import group no longer exists");
    const auto issues = ssh_import_status(*group, hosts); size_t count = 0;
    for (size_t i = 0; i < hosts.size(); ++i) if (issues[i] == SshIssue::None) {
        Check check; check.id = new_id(); check.name = hosts[i].alias; check.kind = Kind::Tcp;
        check.target = hosts[i].target; check.port = hosts[i].port; group->checks.push_back(std::move(check)); ++count;
    }
    const auto error = validate(next); if (!error.empty()) throw std::runtime_error(utf8(error));
    if (count) config = std::move(next);
    return count;
}
}
