#include "cockpit/ipc/transport.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

#define CHECK(x) do { if (!(x)) { std::cerr << __FILE__ << ':' << __LINE__ << " failed: " #x << '\n'; return 1; } } while (false)

int main() {
    using namespace cockpit;
    using namespace std::chrono_literals;
    ipc::BoundedQueue<int> queue(2);
    CHECK(queue.try_push(1) == ipc::QueueStatus::OK);
    CHECK(queue.try_push(2) == ipc::QueueStatus::OK);
    CHECK(queue.try_push(3) == ipc::QueueStatus::FULL);
    CHECK(queue.stats().overflow == 1);
    int value = 0;
    CHECK(queue.try_pop(value) == ipc::QueueStatus::OK && value == 1);
    CHECK(queue.pop_for(value, 10ms) == ipc::QueueStatus::OK && value == 2);
    std::atomic<ipc::QueueStatus> waiter{ipc::QueueStatus::EMPTY};
    std::thread blocked([&] { int out = 0; waiter = queue.pop_for(out, 5s); });
    queue.close();
    blocked.join();
    CHECK(waiter == ipc::QueueStatus::CLOSED);
    CHECK(queue.try_push(4) == ipc::QueueStatus::CLOSED);

    ipc::BoundedQueue<int> full(1);
    CHECK(full.try_push(1) == ipc::QueueStatus::OK);
    CHECK(full.push_for(2, 1ms) == ipc::QueueStatus::TIMEOUT);
    std::atomic<ipc::QueueStatus> blocked_push{ipc::QueueStatus::EMPTY};
    std::thread waiting_producer([&] { blocked_push = full.push_for(3, 5s); });
    full.close();
    waiting_producer.join();
    CHECK(blocked_push == ipc::QueueStatus::CLOSED);

    ipc::BoundedQueue<int> concurrent(4);
    std::atomic<int> consumed{0};
    std::atomic<bool> producer_ok{true};
    std::thread consumer([&] {
        int out = 0;
        while (concurrent.pop_for(out, 50ms) != ipc::QueueStatus::CLOSED) {
            if (out != 0) ++consumed;
            out = 0;
        }
    });
    std::thread producer([&] {
        for (int i = 0; i < 100; ++i) {
            if (concurrent.push_for(i + 1, 1s) != ipc::QueueStatus::OK) producer_ok = false;
        }
    });
    producer.join();
    concurrent.close();
    consumer.join();
    CHECK(producer_ok);
    CHECK(consumed == 100);

    ipc::InMemoryTransport transport(2);
    std::mutex mutex;
    std::condition_variable received;
    int count = 0;
    CHECK(transport.receive([&](const protocol::Message& m) {
        std::lock_guard<std::mutex> lock(mutex);
        if (m.header.message_type == protocol::MessageType::ACK) ++count;
        received.notify_all();
    }).ok());
    CHECK(transport.start().ok());
    protocol::Message message{{protocol::kProtocolVersion, protocol::MessageType::ACK, 0, 1, 1, 1, 0}, {}};
    CHECK(transport.send(message).ok());
    {
        std::unique_lock<std::mutex> lock(mutex);
        CHECK(received.wait_for(lock, 1s, [&] { return count == 1; }));
    }
    transport.stop();
    CHECK(transport.state() == ipc::TransportState::STOPPED);
    CHECK(transport.send(message).code == protocol::StatusCode::INVALID_STATE);
    return 0;
}
