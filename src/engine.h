#pragma once
#include "model.h"
#include <functional>
#include <memory>
#include <mutex>

namespace nw {
struct ManualResult {
    std::mutex mutex;
    bool done{};
    Result result;
};
class Engine {
    struct State;
    std::shared_ptr<State> state;
public:
    explicit Engine(Config config);
    Config configuration() const;
    void apply(Config config);
    void dispatch();
    bool manual(Check check, std::shared_ptr<ManualResult> result);
    Health group_health(const std::wstring& id) const;
    std::wstring tooltip() const;
    unsigned active() const;
};
// The callable must own its data and must not retain host/window pointers.
bool module_work(std::function<void()> work) noexcept;
}
