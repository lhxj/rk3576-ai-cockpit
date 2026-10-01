#pragma once

#include "cockpit/protocol/message.hpp"

#include <atomic>

namespace cockpit::vehicle {

class IClock {
public:
    virtual ~IClock() = default;
    virtual protocol::Deadline now_ms() const = 0;
};

class SystemClock final : public IClock {
public:
    protocol::Deadline now_ms() const override;
};

class FakeClock final : public IClock {
public:
    explicit FakeClock(protocol::Deadline initial_ms) : now_(initial_ms) {}
    protocol::Deadline now_ms() const override { return now_.load(); }
    void set(protocol::Deadline value) { now_.store(value); }
    void advance(protocol::Deadline delta) { now_.fetch_add(delta); }
private:
    std::atomic<protocol::Deadline> now_;
};

}  // namespace cockpit::vehicle
