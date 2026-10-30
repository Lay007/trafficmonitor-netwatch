#include "config.h"
#include "engine.h"
#include "probes.h"
#include <Windows.h>
#include <atomic>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace nw;
static void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static Check make_check(Kind kind = Kind::Icmp) { Check c; c.id = new_id(); c.kind = kind; return c; }
static void wait_idle(Engine& engine) {
    const auto until = Clock::now() + std::chrono::seconds(15);
    while (engine.active() && Clock::now() < until) Sleep(10);
    require(engine.active() == 0, "Engine did not become idle");
}
static void core() {
    require(ui_language(0x419, 0x409) == Language::Russian, "Host Russian must override Windows English");
    require(ui_language(0x409, 0x419) == Language::English, "Host English must override Windows Russian");
    require(ui_language(0, 0x419) == Language::Russian, "Missing host language must use Windows language");
    require(ui_language(0x407, 0x419) == Language::English, "Unsupported host language must use English");
    auto layout = initial_config();
    const auto exported = visible_groups(layout);
    auto edited = layout;
    edited.groups[0].enabled = false; edited.groups[0].checks.push_back(make_check());
    require(!display_changed(exported, edited), "Monitoring edits require no restart");
    edited.groups[0].label = L"Changed"; require(display_changed(exported, edited), "Changed label requires restart");
    edited = layout; edited.groups[0].name = L"Changed"; require(display_changed(exported, edited), "Changed item name requires restart");
    edited = layout; edited.max_visible = 0; require(display_changed(exported, edited), "Zero visible limit requires restart");
    edited = layout; edited.max_visible = 4; require(!display_changed(exported, edited), "Equivalent effective layout requires no restart");
    Group hidden; hidden.id = new_id(); hidden.visible = false; edited.groups.push_back(hidden);
    require(!display_changed(exported, edited), "Adding hidden group requires no restart");
    edited.groups.back().visible = true; require(display_changed(exported, edited), "Adding visible group requires restart");
    auto two_exported = visible_groups(edited); std::swap(edited.groups[0], edited.groups[1]);
    require(display_changed(two_exported, edited), "Visible order change requires restart");
    edited.groups.clear(); require(display_changed(exported, edited), "Removing visible group requires restart");
    require(!display_changed({}, edited), "Empty layout unchanged");
    auto invalid_ru = layout; invalid_ru.groups[0].name.clear();
    require(validate(invalid_ru, Language::Russian).find(L"Название группы") != std::wstring::npos, "Russian validation");
    Check untested;
    require(describe(untested, nullptr, Clock::now(), Language::Russian).find(L"ещё не проверено") != std::wstring::npos, "Russian tooltip");
    Config c = initial_config();
    const auto original = c.groups[0].id;
    Group second; second.id = new_id(); second.name = L"Second"; c.groups.push_back(second);
    c.groups[0].name = L"Renamed \u0441\u0435\u0442\u044c"; c.groups[0].label = L"\u0414\u041e\u041c";
    std::swap(c.groups[0], c.groups[1]); require(c.groups[1].id == original, "Identity changed after reorder");
    c.max_visible = 0; require(visible_groups(c).empty(), "Zero limit");
    c.max_visible = 1; require(visible_groups(c)[0].id == second.id, "First N order");
    c.groups[0].visible = false; require(visible_groups(c)[0].id == original, "Visibility selection");
    c.max_visible.reset(); require(visible_groups(c).size() == 1, "All visible");
    c.groups.erase(c.groups.begin()); require(c.groups.size() == 1 && c.groups[0].id == original, "Delete group");
    auto check = make_check(); c.groups[0].checks.push_back(check);
    require(validate(c).empty(), "Valid config rejected");
    const auto folder = std::filesystem::temp_directory_path() / new_id();
    const auto path = folder / L"NetWatch.conf";
    save_config(path, c);
    auto loaded = load_config(path);
    require(loaded.groups[0].id == original && loaded.groups[0].name == c.groups[0].name && loaded.groups[0].label == c.groups[0].label, "UTF-8 config round trip");
    require(loaded.groups[0].checks[0].id == check.id && !loaded.max_visible, "Check/All round trip");
    {
        std::ifstream file(path, std::ios::binary); std::string bytes((std::istreambuf_iterator<char>(file)), {}); file.close();
        std::string windows = "\xEF\xBB\xBF";
        for (char ch : bytes) { if (ch == '\n') windows += '\r'; windows += ch; }
        std::ofstream output(path, std::ios::binary | std::ios::trunc); output << windows; output.close();
        require(load_config(path).groups[0].name == c.groups[0].name, "Windows BOM/CRLF config");
    }
    c.max_visible = 0; save_config(path, c); require(load_config(path).max_visible == 0, "Persist zero limit");
    auto invalid = c; invalid.groups[0].checks[0].timeout_ms = 0;
    bool rejected = false; try { save_config(path, invalid); } catch (...) { rejected = true; }
    require(rejected && load_config(path).groups[0].checks[0].timeout_ms == 2000, "Invalid save changed file");
    { std::ofstream file(path); file << "NetWatch 1\nmax_visible 0\ngarbage\n"; }
    rejected = false; try { load_config(path); } catch (...) { rejected = true; }
    require(rejected, "Malformed config accepted");
    std::filesystem::remove(path); std::filesystem::remove(folder);
    History h; const auto now = Clock::now(); Result good; good.error = Error::None;
    Result bad; bad.error = Error::Timeout;
    require(health(check, &h, now) == Health::Unknown, "Never tested");
    record(h, bad, now); record(h, bad, now); require(health(check, &h, now) == Health::Unknown, "Failure debounce before success");
    record(h, bad, now); require(health(check, &h, now) == Health::Down, "Third failure");
    record(h, good, now); require(health(check, &h, now) == Health::Up, "One-success recovery");
    record(h, bad, now); require(health(check, &h, now) == Health::Up, "Transient failure after success");
    require(health(check, &h, now + std::chrono::seconds(91)) == Health::Unknown, "Stale green");
    Group group; group.checks = {check}; std::map<std::wstring, History> results{{check.id, h}};
    require(aggregate(group, results, now) == Health::Up, "All successful");
    auto other = make_check(); group.checks.push_back(other);
    require(aggregate(group, results, now) == Health::Unknown, "Incomplete results must not be green");
    auto& down = results[other.id]; record(down, bad, now); record(down, bad, now); record(down, bad, now);
    require(aggregate(group, results, now) == Health::Mixed, "Partial failure");
    group.checks[1].enabled = false; require(aggregate(group, results, now) == Health::Up, "Disabled checks excluded");
    group.enabled = false; require(aggregate(group, results, now) == Health::Unknown, "Disabled group");
    group.enabled = true; group.checks.clear(); require(aggregate(group, results, now) == Health::Unknown, "Empty group");
    std::cout << "PASS model: identities, visibility, config, hysteresis, freshness, aggregation\n";
}
static void expect_probe(Check c, Error expected, const char* message) {
    const auto start = Clock::now(); const auto result = probe(c);
    std::cout << message << ": " << utf8(error_text(result.error)) << " code=" << result.code << " status=" << result.http_status << " ms=" << result.milliseconds << '\n';
    require(result.error == expected, message);
    require(Clock::now() - start < std::chrono::seconds(5), "Probe exceeded deadline tolerance");
}
static void network(int tcp_port, int closed_port, int http_port, int tls_port, int ipv6_port) {
    Check c = make_check(Kind::Tcp); c.timeout_ms = 500;
    c.port = tcp_port; expect_probe(c, Error::None, "TCP loopback open");
    // Windows may retry loopback SYNs for about two seconds before reporting RST.
    c.port = closed_port; c.timeout_ms = 3000; expect_probe(c, Error::Refused, "TCP loopback refused");
    c.timeout_ms = 500;
    c.target = L"::1"; c.port = ipv6_port; expect_probe(c, Error::None, "TCP IPv6 loopback");
    c.kind = Kind::Icmp; c.target = L"127.0.0.1"; expect_probe(c, Error::None, "ICMP IPv4 loopback");
    c.target = L"::1"; expect_probe(c, Error::None, "ICMP IPv6 loopback");
    c.target = L"bad host name"; expect_probe(c, Error::Dns, "ICMP invalid hostname");
    c.target = L"192.0.2.1";
    const auto unreachable = probe(c); require(unreachable.error != Error::None, "ICMP TEST-NET target unexpectedly replied");
    std::cout << "ICMP TEST-NET failure: " << utf8(error_text(unreachable.error)) << " code=" << unreachable.code << '\n';
    c.kind = Kind::Http;
    const auto base = L"http://127.0.0.1:" + std::to_wstring(http_port);
    c.target = base + L"/ok"; expect_probe(c, Error::None, "HTTP 200");
    c.target = base + L"/error"; expect_probe(c, Error::Http, "HTTP 503");
    c.target = base + L"/redirect"; expect_probe(c, Error::Http, "HTTP redirect blocked");
    c.target = base + L"/slow"; expect_probe(c, Error::Timeout, "HTTP total deadline");
    c.target = L"https://127.0.0.1:" + std::to_wstring(tls_port) + L"/ok"; expect_probe(c, Error::Tls, "Untrusted TLS rejected");
    c.target = L"https://127.0.0.1:" + std::to_wstring(tls_port) + L"/downgrade"; expect_probe(c, Error::Tls, "TLS cannot fall back to HTTP");
    Config config;
    for (int i = 0; i < 256; ++i) {
        Group group; group.id = new_id(); group.visible = false;
        Check check = make_check(Kind::Http); check.target = base + L"/slow"; check.timeout_ms = 500;
        group.checks.push_back(check); config.groups.push_back(group);
    }
    Engine engine(config);
    for (int i = 0; i < 20; ++i) engine.dispatch();
    require(engine.active() == 4, "Group count did not respect four-worker cap");
    auto manual = std::make_shared<ManualResult>(); require(!engine.manual(c, manual), "Manual test exceeded cap");
    const auto before_apply = Clock::now();
    engine.apply(initial_config());
    require(Clock::now() - before_apply < std::chrono::milliseconds(100), "Config blocked behind network");
    std::atomic<bool> failed{false};
    std::vector<std::thread> readers;
    const auto id = engine.configuration().groups[0].id;
    for (int i = 0; i < 8; ++i) readers.emplace_back([&] {
        for (int j = 0; j < 100; ++j) {
            if (engine.tooltip().find(L"NetWatch") != 0 || engine.group_health(id) != Health::Unknown) failed = true;
        }
    });
    for (auto& reader : readers) reader.join();
    wait_idle(engine); require(!failed, "Concurrent snapshots/obsolete result publication");
    c.target = base + L"/ok";
    Sleep(1000);
    require(engine.manual(c, manual), "Manual launch failed"); wait_idle(engine);
    { std::lock_guard<std::mutex> lock(manual->mutex); require(manual->done && manual->result.error == Error::None, "Manual result"); }
    std::cout << "PASS engine: 256 hidden groups, four-worker limit, live config, obsolete generations, eight readers, manual tests\n";
}
int main(int argc, char** argv) {
    try {
        if (argc == 7 && std::string(argv[1]) == "network") network(std::stoi(argv[2]), std::stoi(argv[3]), std::stoi(argv[4]), std::stoi(argv[5]), std::stoi(argv[6]));
        else core();
        return 0;
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
