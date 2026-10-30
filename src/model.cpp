#include "model.h"
#include <Windows.h>
#include <objbase.h>
#include <algorithm>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

namespace nw {
Language ui_language(unsigned short host, unsigned short fallback) {
    return PRIMARYLANGID(host ? host : fallback) == LANG_RUSSIAN ? Language::Russian : Language::English;
}
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
std::wstring validate(const Config& config, Language language) {
    if (config.max_visible && *config.max_visible > 4096) return translated(language, L"Display limit must be All or 0-4096.", L"Лимит отображения: Все или число от 0 до 4096.");
    if (config.groups.size() > 4096) return translated(language, L"At most 4096 groups are supported.", L"Допускается не более 4096 групп.");
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
        if (!identity(group.id)) return translated(language, L"Group IDs must be unique UUIDs.", L"Идентификаторы групп должны быть уникальными UUID.");
        if (!valid_text(group.name, 128) || !valid_text(group.label, 24)) return translated(language, L"Group name: 1-128 characters; label: 1-24. No control characters.", L"Название группы: 1-128 символов; метка: 1-24. Управляющие символы запрещены.");
        for (const auto& check : group.checks) {
            if (++count > 16384) return translated(language, L"At most 16384 checks are supported.", L"Допускается не более 16384 проверок.");
            if (!identity(check.id)) return translated(language, L"Check IDs must be unique UUIDs.", L"Идентификаторы проверок должны быть уникальными UUID.");
            if (!valid_text(check.name, 128) || !valid_text(check.target, 2048)) return translated(language, L"Check name and address/URL are required. No control characters.", L"Укажите название и адрес/URL проверки. Управляющие символы запрещены.");
            if (check.interval_ms < 1000 || check.interval_ms > 86400000 || check.timeout_ms < 100 || check.timeout_ms > 30000)
                return translated(language, L"Interval must be 1-86400 seconds; timeout must be 100-30000 milliseconds.", L"Интервал: 1-86400 секунд; тайм-аут: 100-30000 миллисекунд.");
            if (check.kind == Kind::Tcp && (check.port < 1 || check.port > 65535)) return translated(language, L"TCP port must be 1-65535.", L"Порт TCP должен быть от 1 до 65535.");
            if (check.kind != Kind::Icmp && check.kind != Kind::Tcp && check.kind != Kind::Http) return translated(language, L"Unknown check type.", L"Неизвестный тип проверки.");
            if (check.kind == Kind::Http && check.target.rfind(L"https://", 0) != 0 && check.target.rfind(L"http://", 0) != 0)
                return translated(language, L"HTTP checks require an http:// or https:// URL.", L"Для проверки HTTP нужен URL с http:// или https://.");
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
bool display_changed(const std::vector<Group>& exported, const Config& desired) {
    const auto next = visible_groups(desired);
    if (exported.size() != next.size()) return true;
    for (size_t i = 0; i < next.size(); ++i) {
        if (exported[i].id != next[i].id || exported[i].name != next[i].name || exported[i].label != next[i].label) return true;
    }
    return false;
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
const wchar_t* health_text(Health v, Language language) {
    switch (v) {
    case Health::Up: return translated(language, L"Up", L"Доступно");
    case Health::Down: return translated(language, L"Down", L"Недоступно");
    case Health::Mixed: return translated(language, L"Partial failure", L"Частичный сбой");
    default: return translated(language, L"Unknown", L"Нет данных");
    }
}
const wchar_t* error_text(Error v, Language language) {
    switch (v) {
    case Error::None: return translated(language, L"Success", L"Успешно");
    case Error::Dns: return translated(language, L"DNS error", L"Ошибка DNS");
    case Error::Refused: return translated(language, L"Connection refused", L"В подключении отказано");
    case Error::Timeout: return translated(language, L"Timeout", L"Время ожидания истекло");
    case Error::Connect: return translated(language, L"Connection error", L"Ошибка подключения");
    case Error::Unreachable: return translated(language, L"Host unreachable", L"Узел недоступен");
    case Error::Tls: return translated(language, L"TLS validation/handshake error", L"Ошибка сертификата или подключения TLS");
    case Error::Http: return translated(language, L"HTTP status error", L"Ошибка статуса HTTP");
    default: return translated(language, L"Internal error", L"Внутренняя ошибка");
    }
}
const wchar_t* measurement(Kind v, Language language) {
    switch (v) {
    case Kind::Icmp: return L"ICMP RTT";
    case Kind::Tcp: return translated(language, L"TCP connect time", L"Время подключения TCP");
    default: return translated(language, L"HTTP response time (headers)", L"Время ответа HTTP (заголовки)");
    }
}
std::wstring describe(const Check& check, const History* h, Clock::time_point now, Language language) {
    std::wostringstream s;
    s << check.name << L": " << (check.enabled ? health_text(health(check, h, now), language) : translated(language, L"Disabled", L"Отключено"));
    if (h && h->has_result) {
        s << L"; " << error_text(h->last.error, language) << L"; " << measurement(check.kind, language) << L"=";
        if (h->last.has_measurement) s << std::fixed << std::setprecision(1) << h->last.milliseconds << translated(language, L" ms", L" мс");
        else s << translated(language, L"n/a", L"нет данных");
        if (h->last.http_status) s << L"; HTTP " << h->last.http_status;
        if (h->last.code) s << translated(language, L"; code=", L"; код=") << h->last.code;
        if (h->failures) s << translated(language, L"; failures=", L"; ошибок=") << h->failures << L"/3";
        auto time = std::chrono::system_clock::to_time_t(h->last.updated);
        tm local{}; localtime_s(&local, &time);
        s << translated(language, L"; updated ", L"; обновлено ") << std::put_time(&local, L"%Y-%m-%d %H:%M:%S");
    } else s << translated(language, L"; not tested", L"; ещё не проверено");
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
