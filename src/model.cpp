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
    const auto id = host ? host : fallback;
    if (PRIMARYLANGID(id) == LANG_RUSSIAN) return Language::Russian;
    if (PRIMARYLANGID(id) == LANG_CHINESE) {
        const auto region = SUBLANGID(id);
        return id == LANG_CHINESE_TRADITIONAL || region == SUBLANG_CHINESE_TRADITIONAL || region == SUBLANG_CHINESE_HONGKONG || region == SUBLANG_CHINESE_MACAU ?
            Language::ChineseTraditional : Language::ChineseSimplified;
    }
    return Language::English;
}
std::wstring new_id() {
    GUID id{};
    if (FAILED(CoCreateGuid(&id))) throw std::runtime_error("Cannot generate UUID");
    wchar_t text[40]{};
    StringFromGUID2(id, text, 40);
    return std::wstring(text + 1, 36);
}
size_t add_default_groups(Config& config, Language language) {
    auto next = config; size_t count = 0;
    const wchar_t* labels[] = {L"LOCAL", L"WEB", L"SSH"};
    const wchar_t* names[] = {
        translated(language, L"Local devices", L"Локальные устройства", L"本地设备", L"本機裝置"),
        translated(language, L"Web services", L"Веб-сервисы", L"Web 服务", L"Web 服務"), L"SSH"
    };
    for (size_t i = 0; i < 3; ++i) {
        const bool present = std::any_of(next.groups.begin(), next.groups.end(), [&](const Group& g) {
            return CompareStringOrdinal(g.label.c_str(), -1, labels[i], -1, TRUE) == CSTR_EQUAL;
        });
        if (!present) {
            Group group; group.id = new_id(); group.name = names[i]; group.label = labels[i];
            next.groups.push_back(std::move(group)); ++count;
        }
    }
    const auto error = validate(next, language); if (!error.empty()) throw std::runtime_error(utf8(error));
    if (count) config = std::move(next);
    return count;
}
Config initial_config(Language language) {
    Config config; add_default_groups(config, language); return config;
}
static bool valid_text(const std::wstring& value, size_t maximum, bool empty = false) {
    return (empty || !value.empty()) && value.size() <= maximum &&
        std::none_of(value.begin(), value.end(), [](wchar_t c) { return c < 32 || c == 127; });
}
std::wstring validate(const Config& config, Language language) {
    if (config.max_visible && *config.max_visible > 4096) return translated(language, L"Display limit must be All or 0-4096.", L"Лимит отображения: Все или число от 0 до 4096.", L"显示上限必须为全部或 0-4096。", L"顯示上限必須為全部或 0-4096。");
    if (config.groups.size() > 4096) return translated(language, L"At most 4096 groups are supported.", L"Допускается не более 4096 групп.", L"最多支持 4096 个组。", L"最多支援 4096 個群組。");
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
        if (!identity(group.id)) return translated(language, L"Group IDs must be unique UUIDs.", L"Идентификаторы групп должны быть уникальными UUID.", L"组 ID 必须是唯一的 UUID。", L"群組 ID 必須是唯一的 UUID。");
        if (!valid_text(group.name, 128) || !valid_text(group.label, 24)) return translated(language, L"Group name: 1-128 characters; label: 1-24. No control characters.", L"Название группы: 1-128 символов; метка: 1-24. Управляющие символы запрещены.", L"组名长度为 1-128 个字符，标签为 1-24 个字符。不得包含控制字符。", L"群組名稱長度為 1-128 個字元，標籤為 1-24 個字元。不得包含控制字元。");
        for (const auto& check : group.checks) {
            if (++count > 16384) return translated(language, L"At most 16384 checks are supported.", L"Допускается не более 16384 проверок.", L"最多支持 16384 项检测。", L"最多支援 16384 項檢查。");
            if (!identity(check.id)) return translated(language, L"Check IDs must be unique UUIDs.", L"Идентификаторы проверок должны быть уникальными UUID.", L"检测 ID 必须是唯一的 UUID。", L"檢查 ID 必須是唯一的 UUID。");
            if (!valid_text(check.name, 128) || !valid_text(check.target, 2048)) return translated(language, L"Check name and address/URL are required. No control characters.", L"Укажите название и адрес/URL проверки. Управляющие символы запрещены.", L"请填写检测名称和地址/URL，不得包含控制字符。", L"請填寫檢查名稱和位址/URL，不得包含控制字元。");
            if (check.interval_ms < 1000 || check.interval_ms > 86400000 || check.timeout_ms < 100 || check.timeout_ms > 30000)
                return translated(language, L"Interval must be 1-86400 seconds; timeout must be 100-30000 milliseconds.", L"Интервал: 1-86400 секунд; тайм-аут: 100-30000 миллисекунд.", L"间隔必须为 1-86400 秒，超时为 100-30000 毫秒。", L"間隔必須為 1-86400 秒，逾時為 100-30000 毫秒。");
            if (check.kind == Kind::Tcp && (check.port < 1 || check.port > 65535)) return translated(language, L"TCP port must be 1-65535.", L"Порт TCP должен быть от 1 до 65535.", L"TCP 端口必须为 1-65535。", L"TCP 連接埠必須為 1-65535。");
            if (check.kind != Kind::Icmp && check.kind != Kind::Tcp && check.kind != Kind::Http) return translated(language, L"Unknown check type.", L"Неизвестный тип проверки.", L"未知的检测类型。", L"未知的檢查類型。");
            if (check.kind == Kind::Http && check.target.rfind(L"https://", 0) != 0 && check.target.rfind(L"http://", 0) != 0)
                return translated(language, L"HTTP checks require an http:// or https:// URL.", L"Для проверки HTTP нужен URL с http:// или https://.", L"HTTP 检测需要以 http:// 或 https:// 开头的 URL。", L"HTTP 檢查需要以 http:// 或 https:// 開頭的 URL。");
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
    case Health::Up: return translated(language, L"Up", L"Доступно", L"可用", L"可用");
    case Health::Down: return translated(language, L"Down", L"Недоступно", L"不可用", L"不可用");
    case Health::Mixed: return translated(language, L"Partial failure", L"Частичный сбой", L"部分故障", L"部分故障");
    default: return translated(language, L"Unknown", L"Нет данных", L"无数据", L"無資料");
    }
}
const wchar_t* error_text(Error v, Language language) {
    switch (v) {
    case Error::None: return translated(language, L"Success", L"Успешно", L"成功", L"成功");
    case Error::Dns: return translated(language, L"DNS error", L"Ошибка DNS", L"DNS 错误", L"DNS 錯誤");
    case Error::Refused: return translated(language, L"Connection refused", L"В подключении отказано", L"连接被拒绝", L"連線遭拒");
    case Error::Timeout: return translated(language, L"Timeout", L"Время ожидания истекло", L"超时", L"逾時");
    case Error::Connect: return translated(language, L"Connection error", L"Ошибка подключения", L"连接错误", L"連線錯誤");
    case Error::Unreachable: return translated(language, L"Host unreachable", L"Узел недоступен", L"主机不可达", L"主機無法連線");
    case Error::Tls: return translated(language, L"TLS validation/handshake error", L"Ошибка сертификата или подключения TLS", L"TLS 证书或握手错误", L"TLS 憑證或交握錯誤");
    case Error::Http: return translated(language, L"HTTP status error", L"Ошибка статуса HTTP", L"HTTP 状态错误", L"HTTP 狀態錯誤");
    default: return translated(language, L"Internal error", L"Внутренняя ошибка", L"内部错误", L"內部錯誤");
    }
}
const wchar_t* measurement(Kind v, Language language) {
    switch (v) {
    case Kind::Icmp: return L"ICMP RTT";
    case Kind::Tcp: return translated(language, L"TCP connect time", L"Время подключения TCP", L"TCP 连接耗时", L"TCP 連線時間");
    default: return translated(language, L"HTTP response time (headers)", L"Время ответа HTTP (заголовки)", L"HTTP 响应时间（响应头）", L"HTTP 回應時間（標頭）");
    }
}
std::wstring describe(const Check& check, const History* h, Clock::time_point now, Language language) {
    std::wostringstream s;
    s << check.name << L": " << (check.enabled ? health_text(health(check, h, now), language) : translated(language, L"Disabled", L"Отключено", L"已禁用", L"已停用"));
    if (h && h->has_result) {
        s << L"; " << error_text(h->last.error, language) << L"; " << measurement(check.kind, language) << L"=";
        if (h->last.has_measurement) s << std::fixed << std::setprecision(1) << h->last.milliseconds << translated(language, L" ms", L" мс", L" ms", L" ms");
        else s << translated(language, L"n/a", L"нет данных", L"无数据", L"無資料");
        if (h->last.http_status) s << L"; HTTP " << h->last.http_status;
        if (h->last.code) s << translated(language, L"; code=", L"; код=", L"; 错误码=", L"; 錯誤碼=") << h->last.code;
        if (h->failures) s << translated(language, L"; failures=", L"; ошибок=", L"; 连续失败=", L"; 連續失敗=") << h->failures << L"/3";
        auto time = std::chrono::system_clock::to_time_t(h->last.updated);
        tm local{}; localtime_s(&local, &time);
        s << translated(language, L"; updated ", L"; обновлено ", L"; 更新时间 ", L"; 更新時間 ") << std::put_time(&local, L"%Y-%m-%d %H:%M:%S");
    } else s << translated(language, L"; not tested", L"; ещё не проверено", L"; 尚未检测", L"; 尚未檢查");
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
