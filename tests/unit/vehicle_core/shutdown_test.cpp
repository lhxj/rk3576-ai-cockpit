#include "test_support.hpp"

#include <condition_variable>
#include <iostream>

#define CHECK(x) do { if (!(x)) { std::cerr << __FILE__ << ':' << __LINE__ << " failed: " #x << '\n'; return 1; } } while (false)

namespace {
class BlockingMediaAdapter final : public cockpit::vehicle::IServiceAdapter {
public:
    cockpit::vehicle::ServiceDomain domain() const override {
        return cockpit::vehicle::ServiceDomain::MEDIA;
    }
    cockpit::vehicle::DispatchReceipt dispatch(const cockpit::vehicle::VehicleCommand&,
                                                cockpit::vehicle::AdapterCompletion) override {
        std::unique_lock<std::mutex> lock(mutex_);
        entered_ = true;
        entered_cv_.notify_all();
        release_cv_.wait(lock, [&] { return released_; });
        return {cockpit::protocol::Status::Ok(),
                cockpit::vehicle::AdapterResult{cockpit::protocol::Status::Ok(), false}};
    }
    void cancel_request(cockpit::protocol::RequestId) override {}
    void cancel_all() override { release(); }
    bool wait_entered() {
        std::unique_lock<std::mutex> lock(mutex_);
        return entered_cv_.wait_for(lock, std::chrono::seconds(1), [&] { return entered_; });
    }
    void release() {
        std::lock_guard<std::mutex> lock(mutex_);
        released_ = true;
        release_cv_.notify_all();
    }
private:
    std::mutex mutex_;
    std::condition_variable entered_cv_;
    std::condition_variable release_cv_;
    bool entered_{false};
    bool released_{false};
};
}

int main() {
    using namespace cockpit;
    using namespace cockpit::vehicle;
    using namespace cockpit::vehicle::test;

    Fixture repeated(4, false);
    for (int i = 0; i < 20; ++i) {
        CHECK(repeated.core.start().ok());
        CHECK(repeated.core.running());
        repeated.core.stop();
        CHECK(!repeated.core.running());
    }
    repeated.core.stop();

    auto clock = std::make_shared<FakeClock>(1000);
    auto registry = std::make_shared<ServiceRegistry>();
    for (auto domain : {ServiceDomain::MEDIA, ServiceDomain::VOICE, ServiceDomain::RTOS,
                        ServiceDomain::SYSTEM})
        registry->set(domain, ServiceHealth::ONLINE, StateSource::MOCK);
    auto blocking = std::make_shared<BlockingMediaAdapter>();
    auto voice = std::make_shared<MockVoiceAdapter>();
    auto rtos = std::make_shared<MockRtosAdapter>();
    auto system = std::make_shared<MockSystemAdapter>();
    VehicleCore core({88, 1, 4, 16, std::chrono::milliseconds(1)},
                     {blocking, voice, rtos, system}, registry, clock);
    CHECK(core.start().ok());
    InProcessVehicleCoreClient client(core);
    auto make = [&](protocol::RequestId id) {
        VehicleCommand command;
        command.request_id = id;
        command.boot_epoch = 88;
        command.deadline_ms = 1200;
        command.source = CommandSource::TEST;
        command.command_type = CommandType::CAMERA_SNAPSHOT;
        return command;
    };
    auto first = client.send_command(make(1));
    CHECK(first.accepted());
    CHECK(blocking->wait_entered());
    auto queued = client.send_command(make(2));
    CHECK(queued.accepted());
    auto full = client.send_command(make(3));
    CHECK(full.status.code == protocol::StatusCode::UNAVAILABLE);
    CHECK(full.status.detail == "command queue full");
    blocking->release();
    CHECK(first.result.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    CHECK(queued.result.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    core.stop();

    Fixture pending_fixture;
    pending_fixture.media->set_behavior(CommandType::CAMERA_SNAPSHOT, MockBehavior::TIMEOUT);
    auto pending = pending_fixture.client.send_command(
        pending_fixture.command(300, CommandType::CAMERA_SNAPSHOT));
    CHECK(pending.accepted());
    pending_fixture.core.stop();
    CHECK(pending.result.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    CHECK(pending.result.get().status.code == protocol::StatusCode::CANCELLED);
    return 0;
}
