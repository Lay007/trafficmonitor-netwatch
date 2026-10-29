#include "probes.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <winhttp.h>
#include <atomic>
#include <memory>
#include <stdexcept>

namespace nw {
namespace {
struct Handle {
    HANDLE value{};
    explicit Handle(HANDLE v = nullptr) : value(v) {}
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle(const Handle&) = delete;
};
struct Winsock {
    bool ready{};
    Winsock() { WSADATA data{}; ready = WSAStartup(MAKEWORD(2, 2), &data) == 0; }
    ~Winsock() { if (ready) WSACleanup(); }
};
struct Deadline {
    Clock::time_point start{Clock::now()};
    unsigned budget;
    DWORD remaining() const {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
        return elapsed >= budget ? 0 : budget - static_cast<DWORD>(elapsed);
    }
    double elapsed() const { return std::chrono::duration<double, std::milli>(Clock::now() - start).count(); }
};
Error socket_error(int error) {
    if (error == WSAECONNREFUSED) return Error::Refused;
    if (error == WSAETIMEDOUT || error == WAIT_TIMEOUT) return Error::Timeout;
    if (error == WSAEHOSTUNREACH || error == WSAENETUNREACH) return Error::Unreachable;
    return Error::Connect;
}
std::vector<sockaddr_storage> resolve(const Check& check, Deadline& deadline, Result& result) {
    ADDRINFOEXW hints{}; hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM; hints.ai_protocol = IPPROTO_TCP;
    PADDRINFOEXW found = nullptr;
    OVERLAPPED operation{};
    Handle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!event.value) { result.code = GetLastError(); return {}; }
    operation.hEvent = event.value;
    HANDLE cancellation{};
    const auto port = std::to_wstring(check.port);
    int status = GetAddrInfoExW(check.target.c_str(), port.c_str(), NS_DNS, nullptr, &hints, &found, nullptr, &operation, nullptr, &cancellation);
    bool timed_out = false;
    if (status == WSA_IO_PENDING) {
        timed_out = WaitForSingleObject(event.value, deadline.remaining()) != WAIT_OBJECT_0;
        if (timed_out) {
            GetAddrInfoExCancel(&cancellation);
            // The provider must finish touching OVERLAPPED and result before they are destroyed.
            WaitForSingleObject(event.value, INFINITE);
        }
        status = GetAddrInfoExOverlappedResult(&operation);
    }
    std::unique_ptr<ADDRINFOEXW, decltype(&FreeAddrInfoExW)> owned(found, FreeAddrInfoExW);
    if (timed_out || !deadline.remaining()) { result.error = Error::Timeout; result.code = WSAETIMEDOUT; return {}; }
    if (status) { result.error = Error::Dns; result.code = status; return {}; }
    std::vector<sockaddr_storage> addresses;
    for (auto p = found; p; p = p->ai_next) {
        if ((p->ai_family == AF_INET || p->ai_family == AF_INET6) && p->ai_addrlen <= sizeof(sockaddr_storage)) {
            sockaddr_storage address{}; memcpy(&address, p->ai_addr, p->ai_addrlen); addresses.push_back(address);
        }
    }
    if (addresses.empty()) { result.error = Error::Dns; result.code = WSAHOST_NOT_FOUND; }
    return addresses;
}
Result tcp(const Check& check, Deadline& deadline) {
    Result result;
    const auto addresses = resolve(check, deadline, result);
    for (const auto& address : addresses) {
        if (!deadline.remaining()) { result.error = Error::Timeout; break; }
        SOCKET socket = WSASocketW(address.ss_family, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
        if (socket == INVALID_SOCKET) { result.code = WSAGetLastError(); result.error = Error::Connect; continue; }
        Handle event(WSACreateEvent());
        const auto start = Clock::now();
        int error = 0;
        if (!event.value || WSAEventSelect(socket, event.value, FD_CONNECT) == SOCKET_ERROR) error = WSAGetLastError();
        else if (connect(socket, reinterpret_cast<const sockaddr*>(&address), address.ss_family == AF_INET ? sizeof(sockaddr_in) : sizeof(sockaddr_in6)) == SOCKET_ERROR) {
            error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
                const DWORD wait = WaitForSingleObject(event.value, deadline.remaining());
                if (wait != WAIT_OBJECT_0) error = wait == WAIT_TIMEOUT ? WSAETIMEDOUT : WSAGetLastError();
                else {
                    WSANETWORKEVENTS events{};
                    error = WSAEnumNetworkEvents(socket, event.value, &events) == 0 ? events.iErrorCode[FD_CONNECT_BIT] : WSAGetLastError();
                }
            }
        }
        closesocket(socket);
        result.milliseconds = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        result.code = error; result.error = error ? socket_error(error) : Error::None;
        result.has_measurement = !error;
        if (!error) break;
    }
    return result;
}
Result icmp(const Check& check, Deadline& deadline) {
    Result result;
    const auto addresses = resolve(check, deadline, result);
    for (const auto& address : addresses) {
        if (!deadline.remaining()) { result.error = Error::Timeout; break; }
        HANDLE handle = address.ss_family == AF_INET ? IcmpCreateFile() : Icmp6CreateFile();
        if (handle == INVALID_HANDLE_VALUE) { result.code = GetLastError(); continue; }
        char payload[] = "NetWatch";
        alignas(ICMP_ECHO_REPLY) unsigned char reply[512]{};
        DWORD replies;
        if (address.ss_family == AF_INET) {
            replies = IcmpSendEcho(handle, reinterpret_cast<const sockaddr_in*>(&address)->sin_addr.S_un.S_addr,
                payload, sizeof(payload), nullptr, reply, sizeof(reply), deadline.remaining());
        } else {
            sockaddr_in6 source{}; source.sin6_family = AF_INET6;
            auto destination = *reinterpret_cast<const sockaddr_in6*>(&address);
            replies = Icmp6SendEcho2(handle, nullptr, nullptr, nullptr, &source, &destination, payload, sizeof(payload), nullptr, reply, sizeof(reply), deadline.remaining());
        }
        DWORD code = GetLastError();
        if (replies) {
            if (address.ss_family == AF_INET) {
                const auto& echo = *reinterpret_cast<ICMP_ECHO_REPLY*>(reply);
                code = echo.Status; result.milliseconds = echo.RoundTripTime;
            } else {
                const auto& echo = *reinterpret_cast<ICMPV6_ECHO_REPLY*>(reply);
                code = echo.Status; result.milliseconds = echo.RoundTripTime;
            }
        }
        IcmpCloseHandle(handle);
        result.code = code;
        result.error = code == IP_SUCCESS ? Error::None : code == IP_REQ_TIMED_OUT ? Error::Timeout : Error::Unreachable;
        result.has_measurement = result.error == Error::None;
        if (result.error == Error::None) break;
    }
    return result;
}
class HttpRequest {
    Handle ready{CreateEventW(nullptr, FALSE, FALSE, nullptr)};
    Handle drained{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    HINTERNET session{}, connection{}, request{};
    bool drain_installed{};
    std::atomic<DWORD> status{}, error{};
    static void CALLBACK callback(HINTERNET, DWORD_PTR context, DWORD notification, void* information, DWORD) noexcept {
        auto& self = *reinterpret_cast<HttpRequest*>(context);
        if (notification == WINHTTP_CALLBACK_STATUS_REQUEST_ERROR)
            self.error.store(static_cast<WINHTTP_ASYNC_RESULT*>(information)->dwError);
        self.status.store(notification);
        SetEvent(self.ready.value);
    }
    DWORD wait(BOOL accepted, DWORD expected, Deadline& deadline) {
        if (!accepted) return GetLastError();
        DWORD remaining = deadline.remaining();
        if (!remaining) return ERROR_WINHTTP_TIMEOUT;
        DWORD waited = WaitForSingleObject(ready.value, remaining);
        if (waited == WAIT_TIMEOUT || !deadline.remaining()) return ERROR_WINHTTP_TIMEOUT;
        if (waited != WAIT_OBJECT_0) return GetLastError();
        return status == WINHTTP_CALLBACK_STATUS_REQUEST_ERROR ? error.load() : status == expected ? ERROR_SUCCESS : ERROR_WINHTTP_INTERNAL_ERROR;
    }
public:
    ~HttpRequest() {
        if (request) WinHttpCloseHandle(request);
        if (connection) WinHttpCloseHandle(connection);
        if (session) WinHttpCloseHandle(session);
        // WinHTTP signals this session event after its last callback returns.
        if (drain_installed) WaitForSingleObject(drained.value, INFINITE);
    }
    DWORD run(const Check& check, Deadline& deadline, Result& result) {
        if (!ready.value || !drained.value) return ERROR_NOT_ENOUGH_MEMORY;
        URL_COMPONENTS parts{}; parts.dwStructSize = sizeof(parts);
        parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength = parts.dwUserNameLength = parts.dwPasswordLength = static_cast<DWORD>(-1);
        if (!WinHttpCrackUrl(check.target.c_str(), 0, 0, &parts)) return GetLastError();
        if (parts.nScheme != INTERNET_SCHEME_HTTP && parts.nScheme != INTERNET_SCHEME_HTTPS) return ERROR_WINHTTP_UNRECOGNIZED_SCHEME;
        if (parts.dwUserNameLength || parts.dwPasswordLength) return ERROR_INVALID_PARAMETER;
        std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
        std::wstring path = parts.dwUrlPathLength ? std::wstring(parts.lpszUrlPath, parts.dwUrlPathLength) : L"/";
        if (parts.dwExtraInfoLength) path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
        const auto fragment = path.find(L'#'); if (fragment != std::wstring::npos) path.resize(fragment);
        session = WinHttpOpen(L"TrafficMonitor-NetWatch/0.1", WINHTTP_ACCESS_TYPE_NO_PROXY, nullptr, nullptr, WINHTTP_FLAG_ASYNC);
        if (!session) return GetLastError();
        HANDLE notification = drained.value;
        if (!WinHttpSetOption(session, WINHTTP_OPTION_UNLOAD_NOTIFY_EVENT, &notification, sizeof(notification))) return GetLastError();
        drain_installed = true;
        if (!WinHttpSetTimeouts(session, check.timeout_ms, check.timeout_ms, check.timeout_ms, check.timeout_ms)) return GetLastError();
        DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
        if (!WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols))) {
            protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
            if (!WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols))) return GetLastError();
        }
        connection = WinHttpConnect(session, host.c_str(), parts.nPort, 0);
        if (!connection) return GetLastError();
        request = WinHttpOpenRequest(connection, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
            parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
        if (!request) return GetLastError();
        DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        DWORD auto_logon = WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
        if (!WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirects, sizeof(redirects)) ||
            !WinHttpSetOption(request, WINHTTP_OPTION_AUTOLOGON_POLICY, &auto_logon, sizeof(auto_logon))) return GetLastError();
        if (WinHttpSetStatusCallback(request, callback, WINHTTP_CALLBACK_FLAG_SENDREQUEST_COMPLETE | WINHTTP_CALLBACK_FLAG_HEADERS_AVAILABLE | WINHTTP_CALLBACK_FLAG_REQUEST_ERROR, 0) == WINHTTP_INVALID_STATUS_CALLBACK) return GetLastError();
        DWORD code = wait(WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, reinterpret_cast<DWORD_PTR>(this)), WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE, deadline);
        if (!code) code = wait(WinHttpReceiveResponse(request, nullptr), WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE, deadline);
        if (code) return code;
        DWORD size = sizeof(result.http_status);
        if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &result.http_status, &size, WINHTTP_NO_HEADER_INDEX)) return GetLastError();
        return ERROR_SUCCESS;
    }
};
Result http(const Check& check, Deadline& deadline) {
    Result result;
    {
        HttpRequest request;
        result.code = request.run(check, deadline, result);
        result.milliseconds = deadline.elapsed();
    }
    switch (result.code) {
    case ERROR_SUCCESS: result.error = result.http_status >= 200 && result.http_status <= 299 ? Error::None : Error::Http; break;
    case ERROR_WINHTTP_TIMEOUT: result.error = Error::Timeout; break;
    case ERROR_WINHTTP_NAME_NOT_RESOLVED: result.error = Error::Dns; break;
    case ERROR_WINHTTP_SECURE_FAILURE: case ERROR_WINHTTP_SECURE_CERT_DATE_INVALID:
    case ERROR_WINHTTP_SECURE_CERT_CN_INVALID: case ERROR_WINHTTP_SECURE_INVALID_CA:
    case ERROR_WINHTTP_SECURE_CHANNEL_ERROR: result.error = Error::Tls; break;
    case ERROR_WINHTTP_CANNOT_CONNECT: case ERROR_WINHTTP_CONNECTION_ERROR: result.error = Error::Connect; break;
    default: result.error = Error::Internal;
    }
    result.has_measurement = result.http_status != 0;
    return result;
}
}
Result probe(const Check& check) {
    Deadline deadline{Clock::now(), check.timeout_ms};
    Result result;
    try {
        Winsock sockets;
        if (!sockets.ready) result.code = WSASYSNOTREADY;
        else switch (check.kind) {
        case Kind::Icmp: result = icmp(check, deadline); break;
        case Kind::Tcp: result = tcp(check, deadline); break;
        case Kind::Http: result = http(check, deadline); break;
        }
    } catch (...) { result.error = Error::Internal; result.code = ERROR_NOT_ENOUGH_MEMORY; }
    result.updated = std::chrono::system_clock::now();
    return result;
}
}
