#pragma once
#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace nw {
using Clock = std::chrono::steady_clock;
enum class Kind { Icmp, Tcp, Http };
enum class Health { Unknown, Up, Mixed, Down };
enum class Error { None, Dns, Refused, Timeout, Connect, Unreachable, Tls, Http, Internal };
enum class Language { English, Russian };
Language ui_language(unsigned short host, unsigned short fallback);
inline const wchar_t* translated(Language language, const wchar_t* english, const wchar_t* russian) {
    return language == Language::Russian ? russian : english;
}
struct Check {
    std::wstring id, name{L"New check"}, target{L"127.0.0.1"};
    Kind kind{Kind::Icmp};
    unsigned port{80}, interval_ms{30000}, timeout_ms{2000};
    bool enabled{true};
};
struct Group {
    std::wstring id, name{L"New group"}, label{L"GROUP"};
    bool enabled{true}, visible{true};
    std::vector<Check> checks;
};
struct Config {
    std::optional<unsigned> max_visible;
    std::vector<Group> groups;
};
struct Result {
    Error error{Error::Internal};
    double milliseconds{};
    bool has_measurement{};
    unsigned code{}, http_status{};
    std::chrono::system_clock::time_point updated{};
};
struct History {
    Result last;
    Clock::time_point received{}, last_success{};
    unsigned failures{};
    bool ever_succeeded{}, has_result{};
};
std::wstring new_id();
Config initial_config();
std::wstring validate(const Config& config, Language language = Language::English);
std::vector<Group> visible_groups(const Config& config);
bool display_changed(const std::vector<Group>& exported, const Config& desired);
void record(History& history, const Result& result, Clock::time_point now);
Health health(const Check& check, const History* history, Clock::time_point now);
Health aggregate(const Group& group, const std::map<std::wstring, History>& results, Clock::time_point now);
const wchar_t* health_text(Health value, Language language = Language::English);
const wchar_t* error_text(Error value, Language language = Language::English);
const wchar_t* measurement(Kind value, Language language = Language::English);
std::wstring describe(const Check& check, const History* history, Clock::time_point now, Language language = Language::English);
std::string utf8(const std::wstring& text);
std::wstring wide(const std::string& text);
}
