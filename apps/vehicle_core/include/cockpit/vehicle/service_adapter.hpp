#pragma once

#include "cockpit/vehicle/command.hpp"
#include "cockpit/vehicle/state.hpp"

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace cockpit::vehicle {

struct AdapterResult {
    protocol::Status status;
    bool simulated{false};
};

using AdapterCompletion = std::function<void(AdapterResult)>;

struct DispatchReceipt {
    protocol::Status status;
    std::optional<AdapterResult> immediate;
};

class IServiceAdapter {
public:
    virtual ~IServiceAdapter() = default;
    virtual ServiceDomain domain() const = 0;
    virtual DispatchReceipt dispatch(const VehicleCommand& command, AdapterCompletion completion) = 0;
    virtual void cancel_request(protocol::RequestId request_id) = 0;
    virtual void cancel_all() = 0;
};

enum class MockBehavior { SUCCESS, FAILURE, TIMEOUT };

class MockServiceAdapter : public IServiceAdapter {
public:
    explicit MockServiceAdapter(ServiceDomain domain, bool simulated_results = false);
    ServiceDomain domain() const override { return domain_; }
    DispatchReceipt dispatch(const VehicleCommand& command, AdapterCompletion completion) override;
    void cancel_request(protocol::RequestId request_id) override;
    void cancel_all() override;

    void set_behavior(CommandType type, MockBehavior behavior);
    std::size_t invocation_count(CommandType type) const;
    std::size_t cancellation_count() const;
    protocol::Status complete_pending(protocol::RequestId request_id, protocol::Status status);

private:
    const ServiceDomain domain_;
    const bool simulated_results_;
    mutable std::mutex mutex_;
    std::map<CommandType, MockBehavior> behaviors_;
    std::map<CommandType, std::size_t> invocations_;
    std::unordered_map<protocol::RequestId, AdapterCompletion> pending_;
    std::size_t cancellations_{0};
};

class MockMediaAdapter final : public MockServiceAdapter {
public:
    MockMediaAdapter() : MockServiceAdapter(ServiceDomain::MEDIA) {}
};

class MockVoiceAdapter final : public MockServiceAdapter {
public:
    MockVoiceAdapter() : MockServiceAdapter(ServiceDomain::VOICE) {}
};

class MockRtosAdapter final : public MockServiceAdapter {
public:
    MockRtosAdapter() : MockServiceAdapter(ServiceDomain::RTOS, true) {}
};

class MockSystemAdapter final : public MockServiceAdapter {
public:
    MockSystemAdapter() : MockServiceAdapter(ServiceDomain::SYSTEM) {}
};

struct AdapterSet {
    std::shared_ptr<IServiceAdapter> media;
    std::shared_ptr<IServiceAdapter> voice;
    std::shared_ptr<IServiceAdapter> rtos;
    std::shared_ptr<IServiceAdapter> system;
};

}  // namespace cockpit::vehicle
