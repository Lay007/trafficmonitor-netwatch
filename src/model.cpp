#include "model.h"
#include <Windows.h>
#include <objbase.h>
#include <algorithm>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

namespace nw {
std::wstring new_id() {
    GUID id{};
    if (FAILED(CoCreateGuid(&id))) throw std::runtime_error("Cannot generate UUID");
    wchar_t text[40]{};
    StringFromGUID2(id, text, 40);
    return std::wstring(text + 1, 36);
}
Config initial_config() {
    Group group; group.id = new_id(); group.name = L"Local devices"; group.label = L"LOCAL";
    return {std::nullopt, {group}};
}
static bool valid_text(const std::wstring& value, size_t maximum, bool empty = false) {
    return (empty || !value.empty()) && value.size() <= maximum &&
        std::none_of(value.begin(), value.end(), [](wchar_t c) { return c < 32 || c == 127; });
}
std::wstring validate(const Config& config) {
    if (config.max_visible && *config.max_visible > 4096) return L"Display limit must be All or 0-4096.";
    if (config.groups.size() > 4096) return L"At most 4096 groups are supported.";
    std::set<std::wstring> ids;
    size_t count = 0;
    auto identity = [&](const std::wstring& id) {
        if (id.size() != 36) return false;
        for (size_t i = 0; i < id.size(); ++i) {
            if (i == 8 || i == 13 || i == 18 || i == 23) { if (id[i] != L'-') return false; }
            else if (!((id[i] >= L'0' && id[i] <= L'9') || (id[i] >= L'a' && id[i] <= L'f') || (id[i] >= L'A' && id[i] <= L'F'))) return false;
        }
        return ids.insert(id).second;
    };
    for (const auto& group : config.groups) {
        if (!identity(group.id)) return L"Group IDs must be unique UUIDs.";
        if (!valid_text(group.name, 128) || !valid_text(group.label, 24)) return L"Group name: 1-128 characters; label: 1-24. No control characters.";
        for (const auto& check : group.checks) {
            if (++count > 16384) return L"At most 16384 checks are supported.";
            if (!identity(check.id)) return L"Check IDs must be unique UUIDs.";
            if (!valid_text(check.name, 128) || !valid_text(check.target, 2048)) return L"Check name and address/URL are required. No control characters.";
            if (check.interval_ms < 1000 || check.interval_ms > 86400000 || check.timeout_ms < 100 || check.timeout_ms > 30000)
                return L"Interval must be 1-86400 seconds; timeout must be 100-30000 milliseconds.";
            if (check.kind == Kind::Tcp && (check.port < 1 || check.port > 65535)) return L"TCP port must be 1-65535.";
            if (check.kind != Kind::Icmp && check.kind != Kind::Tcp && check.kind != Kind::Http) return L"Unknown check type.";
            if (check.kind == Kind::Http && check.target.rfind(L"https://", 0) != 0 && check.target.rfind(L"http://", 0) != 0)
                return L"HTTP checks require an http:// or https:// URL.";
        }
    }
    return {};
}
std::vector<Group> visible_groups(const Config& config) {
    std::vector<Group> result;
    for (const auto& group : config.groups) {
        if (config.max_visible && result.size() >= *config.max_visible) break;
        if (group.visible) result.push_back(group);
    }
    return result;
}
void record(History& history, const Result& result, Clock::time_point now) {
    history.last = result; history.received = now; history.has_result = true;
    if (result.error == Error::None) {
        history.failures = 0; history.last_success = now; history.ever_succeeded = true;
    } else history.failures = (std::min)(history.failures + 1, 3u);
}
Health health(const Check& check, const History* h, Clock::time_point now) {
    if (!check.enabled || !h || !h->has_result) return Health::Unknown;
    const auto stale = std::chrono::milliseconds((std::max)(3ull * check.interval_ms, 1ull * check.interval_ms + check.timeout_ms));
    if (now - h->received >= stale) return Health::Unknown;
    if (h->failures >= 3) return Health::Down;
    if (!h->ever_succeeded || now - h->last_success >= stale) return Health::Unknown;
    return Health::Up;
}
Health aggregate(const Group& group, const std::map<std::wstring, History>& results, Clock::time_point now) {
    if (!group.enabled) return Health::Unknown;
    unsigned up = 0, down = 0;
    for (const auto& check : group.checks) {
        if (!check.enabled) continue;
        const auto found = results.find(check.id);
        const auto state = health(check, found == results.end() ? nullptr : &found->second, now);
        if (state == Health::Unknown) return state;
        up += state == Health::Up; down += state == Health::Down;
    }
    if (up && down) return Health::Mixed;
    if (up) return Health::Up;
    if (down) return Health::Down;
    return Health::Unknown;
}
const wchar_t* health_text(Health v) {
    switch (v) { case Health::Up: return L"Up"; case Health::Down: return L"Down"; case Health::Mixed: return L"Partial failure"; default: return L"Unknown"; }
}
const wchar_t* error_text(Error v) {
    switch (v) {
    case Error::None: return L"Success"; case Error::Dns: return L"DNS error";
    case Error::Refused: return L"Connection refused"; case Error::Timeout: return L"Timeout";
    case Error::Connect: return L"Connection error"; case Error::Unreachable: return L"Host unreachable";
    case Error::Tls: return L"TLS validation/handshake error"; case Error::Http: return L"HTTP status error";
    default: return L"Internal error";
    }
}
const wchar_t* measurement(Kind v) {
    switch (v) { case Kind::Icmp: return L"ICMP RTT"; case Kind::Tcp: return L"TCP connect time"; default: return L"HTTP response time (headers)"; }
}
std::wstring describe(const Check& check, const History* h, Clock::time_point now) {
    std::wostringstream s;
    s << check.name << L": " << (check.enabled ? health_text(health(check, h, now)) : L"Disabled");
    if (h && h->has_result) {
        s << L"; " << error_text(h->last.error) << L"; " << measurement(check.kind) << L"=";
        if (h->last.has_measurement) s << std::fixed << std::setprecision(1) << h->last.milliseconds << L" ms";
        else s << L"n/a";
        if (h->last.http_status) s << L"; HTTP " << h->last.http_status;
        if (h->last.code) s << L"; code=" << h->last.code;
        if (h->failures) s << L"; failures=" << h->failures << L"/3";
        auto time = std::chrono::system_clock::to_time_t(h->last.updated);
        tm local{}; localtime_s(&local, &time);
        s << L"; updated " << std::put_time(&local, L"%Y-%m-%d %H:%M:%S");
    } else s << L"; not tested";
    return s.str();
}
std::string utf8(const std::wstring& value) {
    if (value.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (!n) throw std::runtime_error("Invalid Unicode");
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), s.data(), n, nullptr, nullptr);
    return s;
}
std::wstring wide(const std::string& value) {
    if (value.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (!n) throw std::runtime_error("Invalid UTF-8");
    std::wstring s(n, 0);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), s.data(), n);
    return s;
}
}
