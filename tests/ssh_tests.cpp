#include "ssh_import.h"
#include "config.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

static void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static void write(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary); file << text;
    require(static_cast<bool>(file), "Write SSH fixture");
}
static const nw::SshHost& host(const std::vector<nw::SshHost>& hosts, const wchar_t* name) {
    const auto found = std::find_if(hosts.begin(), hosts.end(), [&](const nw::SshHost& h) { return h.alias == name; });
    require(found != hosts.end(), "Missing expected alias"); return *found;
}
int main() {
    const auto root = std::filesystem::canonical(std::filesystem::temp_directory_path()) / (L"NetWatch-ssh-" + nw::new_id());
    try {
        const auto config = root / L".ssh" / L"config";
        write(config, "\xEF\xBB\xBF# UTF-8 BOM and CRLF\r\nInclude fragments/*.conf\r\n"
            "Host first second\r\n HostName = \"192.0.2.10\" # comment\r\n Port=2222\r\n User ignored\r\n IdentityFile not-opened\r\n"
            "Host first\n HostName wrong.example\n Port 4444\n"
            "Host excluded\n HostName 192.0.2.11\n"
            "Host * !excluded\n Port 2200\n"
            "Host plain\n HostName %h.example\n"
            "Host ipv6\n HostName ::1\n"
            "Host scoped\n HostName fe80::1%%12\n"
            "Host jump\n ProxyJump gateway\n"
            "Host command\n ProxyCommand some command that must never run\n"
            "Host canonical\n CanonicalizeHostname yes\n"
            "Host tokens\n HostName ${UNSUPPORTED}\n"
            "Host badtoken\n HostName %n\n"
            "Host badport\n Port 0\n"
            "Host badhost\n HostName \"has spaces\"\n"
            "Host wildcard-* !negative\n HostName ignored\n");
        write(root / L".ssh/fragments/10.conf", "Host included\n HostName 192.0.2.20\n Port 2022\n");
        write(root / L".ssh/fragments/20.conf", "Host included\n HostName wrong.example\n Port 9999\n");
        auto hosts = nw::read_ssh_config(config, root);
        require(host(hosts, L"first").target == L"192.0.2.10" && host(hosts, L"first").port == 2222, "Specific first value precedence");
        require(host(hosts, L"second").port == 2222, "Multiple literal aliases");
        require(host(hosts, L"included").target == L"192.0.2.20" && host(hosts, L"included").port == 2022, "Include lexical order");
        require(host(hosts, L"excluded").port == 22, "Negation and default port");
        require(host(hosts, L"plain").target == L"plain.example" && host(hosts, L"plain").port == 2200, "Wildcard defaults and host token");
        require(host(hosts, L"ipv6").target == L"::1" && host(hosts, L"scoped").target == L"fe80::1%12", "IPv6 and escaped percent");
        require(host(hosts, L"jump").issue == nw::SshIssue::Proxy && host(hosts, L"command").issue == nw::SshIssue::Proxy, "Indirect routes must not become direct checks");
        require(host(hosts, L"canonical").issue == nw::SshIssue::Canonicalization, "Unsupported canonicalization");
        require(host(hosts, L"badtoken").issue == nw::SshIssue::Tokens, "Unsupported tokens");
        require(host(hosts, L"tokens").issue == nw::SshIssue::Tokens, "Unsupported environment expansion");
        require(host(hosts, L"badhost").issue == nw::SshIssue::InvalidHost, "Invalid host");
        // A preceding Host * Port wins even over a later invalid per-host Port.
        require(host(hosts, L"badport").port == 2200, "Global first value precedence");
        require(std::none_of(hosts.begin(), hosts.end(), [](const nw::SshHost& h) { return h.alias.find(L'*') != std::wstring::npos || h.alias == L"negative"; }), "Wildcard/negated patterns became endpoints");

        write(config, "Host bad\n Port 65536\nHost zero\n Port 0\nHost missing\nHost *\n ProxyJump none\n");
        hosts = nw::read_ssh_config(config, root);
        require(host(hosts, L"bad").issue == nw::SshIssue::InvalidPort && host(hosts, L"zero").issue == nw::SshIssue::InvalidPort, "Invalid ports accepted");
        require(host(hosts, L"missing").target == L"missing" && host(hosts, L"missing").port == 22, "Alias fallback");
        write(config, "Host outer\n Include conditional.conf\nHost after\n HostName 192.0.2.30\n");
        write(root / L".ssh/conditional.conf", "Host unreachable-alias\n HostName 192.0.2.31\n");
        hosts = nw::read_ssh_config(config, root);
        require(host(hosts, L"unreachable-alias").issue == nw::SshIssue::Inactive, "Inactive Include leaked aliases");
        write(config, "Include conditional.conf\n Port 2345\nHost parent\n");
        hosts = nw::read_ssh_config(config, root);
        require(host(hosts, L"parent").port == 2345, "Included Host state escaped into parent file");
        const std::string rejected[] = {
            "Match exec \"must-not-run\"\n HostName example\n", "Include config\n",
            "Host \"\"\n", "Host broken\n HostName \"unclosed\n", "Include fragments/*/config\n",
            "Include ${UNSUPPORTED}\n", "Include missing.conf\n", std::string(16385, 'x')
        };
        for (const auto& text : rejected) {
            write(config, text); bool failed = false;
            try { nw::read_ssh_config(config, root); } catch (...) { failed = true; }
            require(failed, "Unsupported/cyclic/malformed configuration was accepted");
        }
        auto defaults = nw::initial_config(nw::Language::Russian);
        require(defaults.groups.size() == 3 && defaults.groups[0].name == L"Локальные устройства" && defaults.groups[1].label == L"WEB" && defaults.groups[2].label == L"SSH", "Default group catalog");
        require(nw::validate(defaults).empty() && std::all_of(defaults.groups.begin(), defaults.groups.end(), [](const nw::Group& g) { return g.checks.empty(); }), "Defaults contain probes or invalid IDs");
        const auto id = defaults.groups[2].id; defaults.groups[2].name = L"Custom";
        require(nw::add_default_groups(defaults, nw::Language::English) == 0 && defaults.groups[2].id == id && defaults.groups[2].name == L"Custom", "Default restoration rewrote existing group");
        defaults.groups.erase(defaults.groups.begin());
        require(nw::add_default_groups(defaults, nw::Language::Russian) == 1 && defaults.groups[1].id == id, "Missing default insertion changed identities");
        std::vector<nw::SshHost> selected{{L"one", L"192.0.2.50", 22}, {L"two", L"192.0.2.50", 22}, {L"proxy", L"elsewhere", 22, nw::SshIssue::Proxy}};
        require(nw::append_ssh_hosts(defaults, id, selected) == 1, "Import did not deduplicate endpoint or skip unsupported host");
        const auto check_id = defaults.groups[1].checks[0].id;
        selected = {{L"one", L"192.0.2.51", 2200}};
        require(nw::append_ssh_hosts(defaults, id, selected) == 0 && defaults.groups[1].checks[0].id == check_id && defaults.groups[1].checks[0].target == L"192.0.2.50", "Reimport overwrote an existing alias");
        nw::save_config(root / L"NetWatch.conf", defaults);
        auto loaded = nw::load_config(root / L"NetWatch.conf");
        require(loaded.groups[1].checks[0].id == check_id && loaded.groups[1].checks[0].kind == nw::Kind::Tcp && loaded.groups[1].checks[0].port == 22, "Imported TCP check persistence");
        loaded.groups.resize(1); nw::save_config(root / L"NetWatch.conf", loaded);
        require(nw::load_config(root / L"NetWatch.conf").groups.size() == 1, "Loading existing config inserted defaults");
        auto full = nw::initial_config(); full.groups.resize(1);
        for (size_t i = 0; i < 16384; ++i) { nw::Check c; c.id = nw::new_id(); full.groups[0].checks.push_back(std::move(c)); }
        const auto unchanged_id = full.groups[0].checks.front().id;
        bool capacity_rejected = false;
        try { nw::append_ssh_hosts(full, full.groups[0].id, {{L"extra", L"192.0.2.60", 22}}); } catch (...) { capacity_rejected = true; }
        require(capacity_rejected && full.groups[0].checks.size() == 16384 && full.groups[0].checks[0].id == unchanged_id, "Import capacity failure changed configuration");
        std::filesystem::remove_all(root);
        std::cout << "PASS OpenSSH parsing, precedence, includes, rejection, duplicate-safe import and empty default groups\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL SSH import: " << error.what() << '\n';
        std::error_code ignored; std::filesystem::remove_all(root, ignored); return 1;
    }
}
