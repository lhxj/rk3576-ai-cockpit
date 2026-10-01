#pragma once

#include "cockpit/media/media_service.hpp"
#include "cockpit/vehicle/service_adapter.hpp"

#include <memory>

namespace cockpit::media {

class RealMediaServiceAdapter final : public vehicle::IServiceAdapter {
public:
    explicit RealMediaServiceAdapter(std::shared_ptr<MediaService> service);
    ~RealMediaServiceAdapter() override;

    vehicle::ServiceDomain domain() const override { return vehicle::ServiceDomain::MEDIA; }
    vehicle::DispatchReceipt dispatch(const vehicle::VehicleCommand& command,
                                      vehicle::AdapterCompletion completion) override;
    void cancel_request(protocol::RequestId request_id) override;
    void cancel_all() override;
    [[nodiscard]] bool supports(vehicle::CommandType type) const override;
    [[nodiscard]] vehicle::StateSource state_source() const override {
        return vehicle::StateSource::RUNTIME;
    }

    [[nodiscard]] std::shared_ptr<MediaService> service() const { return service_; }

private:
    struct CompletionState;
    std::shared_ptr<MediaService> service_;
    std::shared_ptr<CompletionState> completions_;
};

}  // namespace cockpit::media
