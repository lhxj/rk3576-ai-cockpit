#pragma once

#include "cockpit/ipc/bounded_queue.hpp"
#include "cockpit/protocol/message.hpp"

#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace cockpit::ipc {

enum class TransportState { STOPPED, RUNNING, ERROR };
using ReceiveCallback = std::function<void(const protocol::Message&)>;

class ITransport {
public:
    virtual ~ITransport() = default;
    virtual protocol::Status receive(ReceiveCallback callback) = 0;
    virtual protocol::Status start() = 0;
    virtual void stop() = 0;
    virtual protocol::Status send(const protocol::Message& message) = 0;
    virtual TransportState state() const = 0;
    virtual QueueStats stats() const = 0;
};

// Host-only, single-use transport. Callback runs on the one owned worker.
// stop() is called by its owner, never from inside the callback.
class InMemoryTransport final : public ITransport {
public:
    explicit InMemoryTransport(std::size_t capacity);
    ~InMemoryTransport() override;
    protocol::Status receive(ReceiveCallback callback) override;
    protocol::Status start() override;
    void stop() override;
    protocol::Status send(const protocol::Message& message) override;
    TransportState state() const override;
    QueueStats stats() const override { return queue_.stats(); }

private:
    void run();
    mutable std::mutex mutex_;
    BoundedQueue<std::vector<std::uint8_t>> queue_;
    ReceiveCallback callback_;
    TransportState state_{TransportState::STOPPED};
    bool ever_started_{false};
    std::thread worker_;
};

}  // namespace cockpit::ipc
