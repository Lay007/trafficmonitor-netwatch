#pragma once
#include "model.h"
#include <filesystem>

namespace nw {
enum class SshIssue { None, Proxy, Canonicalization, InvalidHost, InvalidPort, Tokens, Inactive, Duplicate };
struct SshHost {
    std::wstring alias, target;
    unsigned port{22};
    SshIssue issue{SshIssue::None};
};
// Reads configuration only. Never launches ssh, Match exec, a proxy or a key agent.
std::vector<SshHost> read_ssh_config(const std::filesystem::path& path, const std::filesystem::path& home);
const wchar_t* ssh_issue_text(SshIssue issue, Language language);
std::vector<SshIssue> ssh_import_status(const Group& group, const std::vector<SshHost>& hosts);
size_t append_ssh_hosts(Config& config, const std::wstring& group_id, const std::vector<SshHost>& hosts);
}
