#include "engine.h"
#include "probes.h"
#include <Windows.h>
#include <algorithm>
#include <stdexcept>

namespace nw {
namespace {
struct Work { HMODULE module; std::function<void()> function; };
DWORD WINAPI run_work(void* argument) noexcept {
    const HMODULE module = static_cast<Work*>(argument)->module;
    {
        std::unique_ptr<Work> work(static_cast<Work*>(argument));
        try { work->function(); } catch (...) { /* Owners publish their own errors. */ }
    }
    FreeLibraryAndExitThread(module, 0);
}
}
bool module_work(std::function<void()> function) noexcept {
    HMODULE module{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCWSTR>(&run_work), &module)) return false;
    try {
        auto work = std::make_unique<Work>(Work{module, std::move(function)});
        HANDLE thread = CreateThread(nullptr, 0, run_work, work.get(), 0, nullptr);
        if (thread) { work.release(); CloseHandle(thread); return true; }
    } catch (...) {}
    FreeLibrary(module);
    return false;
}
struct Engine::State {
    mutable std::mutex mutex;
    Config config;
    struct Runtime { Clock::time_point due{}; bool running{}; };
    std::map<std::wstring, Runtime> runtime;
    std::map<std::wstring, History> results;
    uint64_t generation{};
    unsigned active{};
    Clock::time_point rate_window{};
    unsigned starts{};
    bool reserve() {
        const auto now = Clock::now();
        if (now - rate_window >= std::chrono::seconds(1)) { rate_window = now; starts = 0; }
        if (active >= 4 || starts >= 4) return false;
        ++active; ++starts; return true;
    }
    explicit State(Config c) : config(std::move(c)) {}
};
Engine::Engine(Config config) : state(std::make_shared<State>(std::move(config))) {}
Config Engine::configuration() const {
    std::lock_guard<std::mutex> lock(state->mutex); return state->config;
}
void Engine::apply(Config config) {
    const auto error = validate(config);
    if (!error.empty()) throw std::runtime_error(utf8(error));
    std::lock_guard<std::mutex> lock(state->mutex);
    state->config = std::move(config); ++state->generation;
    state->runtime.clear(); state->results.clear();
}
void Engine::dispatch() {
    for (unsigned i = 0; i < 4; ++i) {
        Check selected; bool found = false; uint64_t generation{};
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (state->active >= 4) return;
            auto earliest = Clock::time_point::max(); const auto now = Clock::now();
            for (const auto& group : state->config.groups) {
                if (!group.enabled) continue;
                for (const auto& check : group.checks) {
                    if (!check.enabled) continue;
                    auto& runtime = state->runtime[check.id];
                    if (!runtime.running && runtime.due <= now && runtime.due < earliest) {
                        selected = check; earliest = runtime.due; found = true;
                    }
                }
            }
            if (!found) return;
            if (!state->reserve()) return;
            state->runtime[selected.id].running = true; generation = state->generation;
        }
        const auto owner = state;
        bool launched = false;
        try {
            launched = module_work([owner, selected, generation] {
                const auto result = probe(selected);
                std::lock_guard<std::mutex> lock(owner->mutex);
                --owner->active;
                if (owner->generation != generation) return;
                auto& runtime = owner->runtime[selected.id]; runtime.running = false;
                runtime.due = Clock::now() + std::chrono::milliseconds(selected.interval_ms);
                record(owner->results[selected.id], result, Clock::now());
            });
        } catch (...) {}
        if (!launched) {
            std::lock_guard<std::mutex> lock(state->mutex); --state->active;
            if (generation == state->generation) {
                auto& runtime = state->runtime[selected.id]; runtime.running = false;
                runtime.due = Clock::now() + std::chrono::seconds(1);
                Result result; result.code = ERROR_NOT_ENOUGH_MEMORY; result.updated = std::chrono::system_clock::now();
                record(state->results[selected.id], result, Clock::now());
            }
        }
    }
}
bool Engine::manual(Check check, std::shared_ptr<ManualResult> output) {
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (!state->reserve()) return false;
    }
    auto owner = state; bool launched = false;
    try {
        launched = module_work([owner, check, output] {
            const auto result = probe(check);
            { std::lock_guard<std::mutex> lock(output->mutex); output->result = result; output->done = true; }
            { std::lock_guard<std::mutex> lock(owner->mutex); --owner->active; }
        });
    } catch (...) {}
    if (!launched) { std::lock_guard<std::mutex> lock(state->mutex); --state->active; }
    return launched;
}
unsigned Engine::active() const { std::lock_guard<std::mutex> lock(state->mutex); return state->active; }
Health Engine::group_health(const std::wstring& id) const {
    std::lock_guard<std::mutex> lock(state->mutex);
    for (const auto& group : state->config.groups) if (group.id == id) return aggregate(group, state->results, Clock::now());
    return Health::Unknown;
}
std::wstring Engine::tooltip() const {
    std::lock_guard<std::mutex> lock(state->mutex);
    std::wstring text = L"NetWatch";
    for (const auto& group : state->config.groups) {
        text += L"\r\n" + group.label + L" (" + group.name + L"): " + (group.enabled ? health_text(aggregate(group, state->results, Clock::now())) : L"Monitoring disabled");
        for (const auto& check : group.checks) {
            const auto found = state->results.find(check.id);
            text += L"\r\n  " + describe(check, found == state->results.end() ? nullptr : &found->second, Clock::now());
            if (text.size() > 12000) { text.resize(12000); return text + L"\r\nMore checks in NetWatch settings."; }
        }
        if (text.size() > 12000) break;
    }
    return text;
}
}
