#include "cockpit/vehicle/core.hpp"

#include "cockpit/ipc/bounded_queue.hpp"

#include <algorithm>
#include <atomic>
#include <deque>
#include <future>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace cockpit::vehicle {
namespace {

struct RequestRecord {
    explicit RequestRecord(VehicleCommand value)
        : command(std::move(value)), fingerprint(command_fingerprint(command)),
          future(promise.get_future().share()) {}
    VehicleCommand command;
    std::string fingerprint;
    std::promise<CommandResult> promise;
    std::shared_future<CommandResult> future;
    bool completed{false};
};

struct CommandEvent { std::shared_ptr<RequestRecord> record; };
struct HealthEvent { ServiceDomain domain; ServiceState state; };
struct TickEvent {};
using ControlEvent = std::variant<CommandEvent, HealthEvent, TickEvent>;

struct CompletionEvent {
    std::uint64_t generation{0};
    protocol::RequestId request_id{0};
    AdapterResult result;
};

struct CompletionPort {
    std::mutex mutex;
    ipc::BoundedQueue<CompletionEvent>* queue{nullptr};
    std::uint64_t generation{0};

    bool post(CompletionEvent event) {
        std::lock_guard<std::mutex> lock(mutex);
        if (queue == nullptr || event.generation != generation) return false;
        return queue->push_for(std::move(event), std::chrono::milliseconds(100)) == ipc::QueueStatus::OK;
    }
};

VehicleState initial_state(const ServiceRegistry& registry) {
    VehicleState state;
    state.front_camera = {CameraAvailability::AVAILABLE, StateCondition::ONLINE, StateSource::MOCK, 0};
    state.rear_camera = {CameraAvailability::UNAVAILABLE, StateCondition::OFFLINE, StateSource::MOCK, 0};
    state.selected_camera = {CameraSelection::FRONT, StateCondition::ONLINE, StateSource::MOCK, 0};
    state.recording = {RecordingState::STOPPED, StateCondition::ONLINE, StateSource::MOCK, 0};
    state.rtsp = {BinaryState::OFF, StateCondition::ONLINE, StateSource::MOCK, 0};
    state.media = {MediaState::STOPPED, StateCondition::ONLINE, StateSource::MOCK, 0};
    state.audio = {BinaryState::OFF, StateCondition::OFFLINE, StateSource::MOCK, 0};
    state.voice = {VoiceState::IDLE, StateCondition::OFFLINE, StateSource::MOCK, 0};
    state.vision = {BinaryState::OFF, StateCondition::OFFLINE, StateSource::MOCK, 0};
    state.language_model = {BinaryState::OFF, StateCondition::OFFLINE, StateSource::MOCK, 0};
    state.rtos = {BinaryState::OFF, StateCondition::OFFLINE, StateSource::MOCK, 0};
    state.sensor = {BinaryState::OFF, StateCondition::OFFLINE, StateSource::MOCK, 0};
    state.simulated_led = {BinaryState::OFF, StateCondition::SIMULATED, StateSource::MOCK, 0};
    state.simulated_buzzer = {BinaryState::OFF, StateCondition::SIMULATED, StateSource::MOCK, 0};
    state.wifi = {BinaryState::OFF, StateCondition::OFFLINE, StateSource::MOCK, 0};
    state.services = registry.snapshot();
    return state;
}

VehicleState initial_state(const std::shared_ptr<ServiceRegistry>& registry) {
    if (registry) return initial_state(*registry);
    ServiceRegistry empty;
    return initial_state(empty);
}

const std::string* parameter(const VehicleCommand& command, const std::string& key) {
    for (const auto& item : command.parameters) if (item.first == key) return &item.second;
    return nullptr;
}

bool is_success(const AdapterResult& result) { return result.status.ok(); }

}  // namespace

class VehicleCore::Impl {
public:
    Impl(VehicleCoreConfig config, AdapterSet adapters,
         std::shared_ptr<ServiceRegistry> registry, std::shared_ptr<IClock> clock)
        : config_(config), adapters_(std::move(adapters)), registry_(std::move(registry)),
          clock_(std::move(clock)), state_store_(initial_state(registry_)),
          completion_port_(std::make_shared<CompletionPort>()) {}

    ~Impl() { stop(); }

    protocol::Status start() {
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);
        if (running_.load()) return {protocol::StatusCode::INVALID_STATE, "core already running"};
        if (config_.boot_epoch == 0 || config_.command_queue_capacity == 0 ||
            config_.completion_queue_capacity == 0 || config_.recent_request_capacity == 0 ||
            !clock_ || !registry_ || !adapters_.media || !adapters_.voice ||
            !adapters_.rtos || !adapters_.system)
            return {protocol::StatusCode::INVALID_ARGUMENT, "vehicle core configuration"};
        command_queue_ = std::make_unique<ipc::BoundedQueue<ControlEvent>>(config_.command_queue_capacity);
        completion_queue_ = std::make_unique<ipc::BoundedQueue<CompletionEvent>>(config_.completion_queue_capacity);
        ++generation_;
        {
            std::lock_guard<std::mutex> port_lock(completion_port_->mutex);
            completion_port_->queue = completion_queue_.get();
            completion_port_->generation = generation_;
        }
        stopping_.store(false);
        state_store_.update([&](VehicleState& state) {
            const auto services = registry_->snapshot();
            if (state.services == services) return false;
            state.services = services;
            return true;
        });
        running_.store(true);
        try {
            worker_ = std::thread(&Impl::run, this, generation_);
        } catch (...) {
            running_.store(false);
            disable_completion_port();
            command_queue_.reset();
            completion_queue_.reset();
            return {protocol::StatusCode::INTERNAL_ERROR, "vehicle core worker start"};
        }
        return protocol::Status::Ok();
    }

    void stop() {
        std::unique_lock<std::mutex> lock(lifecycle_mutex_);
        if (!running_.load() && !worker_.joinable()) return;
        stopping_.store(true);
        running_.store(false);
        adapters_.media->cancel_all();
        adapters_.voice->cancel_all();
        adapters_.rtos->cancel_all();
        adapters_.system->cancel_all();
        disable_completion_port();
        if (command_queue_) command_queue_->close();
        if (completion_queue_) completion_queue_->close();
        lock.unlock();
        if (worker_.joinable()) worker_.join();
        cancel_pending_on_stop();
        lock.lock();
        command_queue_.reset();
        completion_queue_.reset();
    }

    bool running() const { return running_.load(); }

    CommandSubmission send_command(const VehicleCommand& command) {
        std::lock_guard<std::mutex> lifecycle_lock(lifecycle_mutex_);
        if (!running_.load()) return reject({protocol::StatusCode::INVALID_STATE, "vehicle core stopped"});
        auto valid = validate_command_shape(command, config_.boot_epoch, clock_->now_ms());
        if (!valid.ok()) return reject(valid);
        const auto domain = service_for(command.command_type);
        if (!registry_->available(domain))
            return reject({protocol::StatusCode::UNAVAILABLE, "target service unavailable"});

        std::shared_ptr<RequestRecord> record;
        {
            std::lock_guard<std::mutex> lock(request_mutex_);
            const auto existing = requests_.find(command.request_id);
            if (existing != requests_.end()) {
                if (existing->second->fingerprint != command_fingerprint(command))
                    return reject({protocol::StatusCode::DUPLICATE_REQUEST,
                                   "request id reused with different command"});
                CommandSubmission replay;
                replay.status = protocol::Status::Ok();
                replay.disposition = SubmissionDisposition::REPLAYED;
                replay.result = existing->second->future;
                return replay;
            }
            record = std::make_shared<RequestRecord>(command);
            requests_.emplace(command.request_id, record);
        }

        const auto ack_sequence = next_lifecycle_sequence();
        const auto status = command_queue_->try_push(CommandEvent{record});
        if (status != ipc::QueueStatus::OK) {
            std::lock_guard<std::mutex> lock(request_mutex_);
            const auto found = requests_.find(command.request_id);
            if (found != requests_.end() && found->second == record) requests_.erase(found);
            return reject({status == ipc::QueueStatus::FULL ? protocol::StatusCode::UNAVAILABLE
                                                             : protocol::StatusCode::INVALID_STATE,
                           status == ipc::QueueStatus::FULL ? "command queue full" : "command queue closed"});
        }

        CommandSubmission submission;
        submission.status = protocol::Status::Ok();
        submission.disposition = SubmissionDisposition::ACCEPTED;
        submission.ack_emitted = true;
        submission.ack = {command.request_id, command.session_id, command.boot_epoch,
                          ack_sequence, protocol::Status::Ok()};
        submission.result = record->future;
        return submission;
    }

    VehicleState snapshot() const { return state_store_.snapshot(); }

    protocol::Status subscribe(StateCallback callback) {
        if (!callback) return {protocol::StatusCode::INVALID_ARGUMENT, "state callback"};
        std::lock_guard<std::mutex> lock(subscriber_mutex_);
        subscribers_.push_back(std::move(callback));
        return protocol::Status::Ok();
    }

    protocol::Status set_service_health(ServiceDomain domain, ServiceHealth health, StateSource source) {
        std::lock_guard<std::mutex> lifecycle_lock(lifecycle_mutex_);
        if (domain == ServiceDomain::COUNT)
            return {protocol::StatusCode::INVALID_ARGUMENT, "service domain"};
        if (!running_.load()) return {protocol::StatusCode::INVALID_STATE, "vehicle core stopped"};
        const auto previous = registry_->get(domain);
        registry_->set(domain, health, source);
        const auto current = registry_->get(domain);
        const auto queued = command_queue_->try_push(HealthEvent{domain, current});
        if (queued != ipc::QueueStatus::OK) {
            registry_->set(domain, previous.health, previous.source);
            return {protocol::StatusCode::UNAVAILABLE, "command queue full"};
        }
        return protocol::Status::Ok();
    }

    protocol::Status poll_deadlines() {
        std::lock_guard<std::mutex> lifecycle_lock(lifecycle_mutex_);
        if (!running_.load()) return {protocol::StatusCode::INVALID_STATE, "vehicle core stopped"};
        const auto queued = command_queue_->try_push(TickEvent{});
        return queued == ipc::QueueStatus::OK
            ? protocol::Status::Ok()
            : protocol::Status{protocol::StatusCode::UNAVAILABLE, "command queue full"};
    }

    std::uint64_t ignored_late_results() const { return ignored_late_results_.load(); }
    protocol::BootEpoch boot_epoch() const { return config_.boot_epoch; }

private:
    CommandSubmission reject(protocol::Status status) const {
        CommandSubmission submission;
        submission.status = std::move(status);
        return submission;
    }

    std::uint64_t next_lifecycle_sequence() { return lifecycle_sequence_.fetch_add(1) + 1; }

    void disable_completion_port() {
        std::lock_guard<std::mutex> lock(completion_port_->mutex);
        completion_port_->queue = nullptr;
    }

    IServiceAdapter* adapter(ServiceDomain domain) const {
        switch (domain) {
            case ServiceDomain::MEDIA: return adapters_.media.get();
            case ServiceDomain::VOICE: return adapters_.voice.get();
            case ServiceDomain::RTOS: return adapters_.rtos.get();
            case ServiceDomain::SYSTEM: return adapters_.system.get();
            case ServiceDomain::INFER:
            case ServiceDomain::COUNT: return nullptr;
        }
        return nullptr;
    }

    void run(std::uint64_t generation) {
        while (!stopping_.load()) {
            CompletionEvent completion;
            if (completion_queue_->try_pop(completion) == ipc::QueueStatus::OK) {
                process_completion(completion, generation);
                check_deadlines();
                continue;
            }
            ControlEvent event;
            const auto status = command_queue_->pop_for(event, config_.poll_interval);
            if (stopping_.load()) break;
            if (status == ipc::QueueStatus::OK) process_control(event, generation);
            else if (status == ipc::QueueStatus::CLOSED) break;
            check_deadlines();
        }
    }

    void process_control(const ControlEvent& event, std::uint64_t generation) {
        if (const auto* command = std::get_if<CommandEvent>(&event)) {
            process_command(command->record, generation);
        } else if (const auto* health = std::get_if<HealthEvent>(&event)) {
            const auto changed = state_store_.update([&](VehicleState& state) {
                auto& target = state.services.at(static_cast<std::size_t>(health->domain));
                if (target.health == health->state.health && target.source == health->state.source &&
                    target.sequence == health->state.sequence) return false;
                target = health->state;
                return true;
            });
            if (changed) publish_state();
        }
    }

    void process_command(const std::shared_ptr<RequestRecord>& record, std::uint64_t generation) {
        if (is_completed(record)) return;
        if (clock_->now_ms() > record->command.deadline_ms) {
            finish(record, {{protocol::StatusCode::TIMEOUT, "deadline before dispatch"}, false});
            return;
        }
        const auto& command = record->command;
        if (command.command_type == CommandType::QUERY_STATE) {
            finish(record, {protocol::Status::Ok(), false});
            return;
        }
        if (command.command_type == CommandType::CAMERA_SELECT) {
            const auto* camera = parameter(command, "camera");
            if (camera != nullptr && *camera == "rear" &&
                snapshot().rear_camera.value != CameraAvailability::AVAILABLE) {
                finish(record, {{protocol::StatusCode::UNAVAILABLE, "rear camera unavailable"}, false});
                return;
            }
        }
        if (command.command_type == CommandType::RECORDING_START &&
            snapshot().recording.value == RecordingState::RECORDING) {
            finish(record, {protocol::Status::Ok(), false});
            return;
        }
        if (command.command_type == CommandType::RECORDING_STOP &&
            snapshot().recording.value == RecordingState::STOPPED) {
            finish(record, {protocol::Status::Ok(), false});
            return;
        }
        if (command.command_type == CommandType::VOICE_SESSION_CANCEL) cancel_voice_session(command);

        apply_dispatch_state(command);
        auto* target = adapter(service_for(command.command_type));
        if (target == nullptr) {
            finish(record, {{protocol::StatusCode::UNAVAILABLE, "adapter missing"}, false});
            return;
        }
        const auto port = completion_port_;
        const auto request_id = command.request_id;
        auto receipt = target->dispatch(command, [port, generation, request_id](AdapterResult result) {
            port->post({generation, request_id, std::move(result)});
        });
        if (!receipt.status.ok()) {
            finish(record, {receipt.status, false});
        } else if (receipt.immediate.has_value()) {
            finish(record, *receipt.immediate);
        }
    }

    void cancel_voice_session(const VehicleCommand& cancel) {
        std::vector<std::shared_ptr<RequestRecord>> targets;
        {
            std::lock_guard<std::mutex> lock(request_mutex_);
            for (const auto& item : requests_) {
                const auto& record = item.second;
                if (!record->completed && record->command.command_type == CommandType::VOICE_SESSION_START &&
                    record->command.session_id == cancel.session_id) targets.push_back(record);
            }
        }
        for (const auto& target : targets) {
            adapters_.voice->cancel_request(target->command.request_id);
            finish(target, {{protocol::StatusCode::CANCELLED, "voice session cancelled"}, false});
        }
    }

    void process_completion(const CompletionEvent& event, std::uint64_t generation) {
        if (event.generation != generation) {
            ++ignored_late_results_;
            return;
        }
        auto record = find_request(event.request_id);
        if (!record || is_completed(record)) {
            ++ignored_late_results_;
            return;
        }
        finish(record, event.result);
    }

    void check_deadlines() {
        const auto now = clock_->now_ms();
        std::vector<std::shared_ptr<RequestRecord>> expired;
        {
            std::lock_guard<std::mutex> lock(request_mutex_);
            for (const auto& item : requests_) {
                if (!item.second->completed && now > item.second->command.deadline_ms)
                    expired.push_back(item.second);
            }
        }
        for (const auto& record : expired)
            finish(record, {{protocol::StatusCode::TIMEOUT, "service result deadline"}, false});
    }

    std::shared_ptr<RequestRecord> find_request(protocol::RequestId id) const {
        std::lock_guard<std::mutex> lock(request_mutex_);
        const auto found = requests_.find(id);
        return found == requests_.end() ? nullptr : found->second;
    }

    bool is_completed(const std::shared_ptr<RequestRecord>& record) const {
        std::lock_guard<std::mutex> lock(request_mutex_);
        return record->completed;
    }

    void finish(const std::shared_ptr<RequestRecord>& record, AdapterResult adapter_result) {
        {
            std::lock_guard<std::mutex> lock(request_mutex_);
            if (record->completed) {
                ++ignored_late_results_;
                return;
            }
        }
        apply_result_state(record->command, adapter_result);
        CommandResult result{record->command.request_id, record->command.session_id,
                             record->command.boot_epoch, next_lifecycle_sequence(),
                             adapter_result.status, adapter_result.simulated};
        {
            std::lock_guard<std::mutex> lock(request_mutex_);
            if (record->completed) {
                ++ignored_late_results_;
                return;
            }
            record->completed = true;
            completed_order_.push_back(record->command.request_id);
            while (completed_order_.size() > config_.recent_request_capacity) {
                const auto evict = completed_order_.front();
                completed_order_.pop_front();
                const auto found = requests_.find(evict);
                if (found != requests_.end() && found->second->completed) requests_.erase(found);
            }
        }
        record->promise.set_value(std::move(result));
    }

    void cancel_pending_on_stop() {
        std::vector<std::shared_ptr<RequestRecord>> pending;
        {
            std::lock_guard<std::mutex> lock(request_mutex_);
            for (const auto& item : requests_) if (!item.second->completed) pending.push_back(item.second);
        }
        for (const auto& record : pending) {
            CommandResult result{record->command.request_id, record->command.session_id,
                                 record->command.boot_epoch, next_lifecycle_sequence(),
                                 {protocol::StatusCode::CANCELLED, "vehicle core stopped"}, false};
            {
                std::lock_guard<std::mutex> lock(request_mutex_);
                if (record->completed) continue;
                record->completed = true;
                completed_order_.push_back(record->command.request_id);
            }
            record->promise.set_value(std::move(result));
        }
    }

    void apply_dispatch_state(const VehicleCommand& command) {
        bool changed = false;
        if (command.command_type == CommandType::RECORDING_START) {
            changed = state_store_.update([](VehicleState& state) {
                return set_state_value(state, state.recording, RecordingState::STARTING,
                                       StateCondition::STARTING, StateSource::MOCK);
            });
        } else if (command.command_type == CommandType::RECORDING_STOP) {
            changed = state_store_.update([](VehicleState& state) {
                return set_state_value(state, state.recording, RecordingState::STOPPING,
                                       StateCondition::STARTING, StateSource::MOCK);
            });
        } else if (command.command_type == CommandType::VOICE_SESSION_START) {
            changed = state_store_.update([](VehicleState& state) {
                return set_state_value(state, state.voice, VoiceState::STARTING,
                                       StateCondition::STARTING, StateSource::MOCK);
            });
        } else if (command.command_type == CommandType::VOICE_SESSION_CANCEL) {
            changed = state_store_.update([](VehicleState& state) {
                return set_state_value(state, state.voice, VoiceState::CANCELLING,
                                       StateCondition::STARTING, StateSource::MOCK);
            });
        }
        if (changed) publish_state();
    }

    void apply_result_state(const VehicleCommand& command, const AdapterResult& result) {
        const bool success = is_success(result);
        const auto changed = state_store_.update([&](VehicleState& state) {
            if (!success) {
                if (command.command_type == CommandType::RECORDING_START ||
                    command.command_type == CommandType::RECORDING_STOP)
                    return set_state_value(state, state.recording, RecordingState::ERROR,
                                           StateCondition::ERROR, StateSource::MOCK);
                if (command.command_type == CommandType::VOICE_SESSION_START)
                    return set_state_value(state, state.voice,
                                           result.status.code == protocol::StatusCode::CANCELLED
                                               ? VoiceState::IDLE : VoiceState::ERROR,
                                           result.status.code == protocol::StatusCode::CANCELLED
                                               ? StateCondition::OFFLINE : StateCondition::ERROR,
                                           StateSource::MOCK);
                if (command.command_type == CommandType::VOICE_SESSION_CANCEL)
                    return set_state_value(state, state.voice, VoiceState::ERROR,
                                           StateCondition::ERROR, StateSource::MOCK);
                return false;
            }
            switch (command.command_type) {
                case CommandType::CAMERA_SELECT: {
                    const auto* selected = parameter(command, "camera");
                    return set_state_value(state, state.selected_camera,
                        selected != nullptr && *selected == "rear" ? CameraSelection::REAR : CameraSelection::FRONT,
                        StateCondition::ONLINE, StateSource::MOCK);
                }
                case CommandType::RECORDING_START:
                    return set_state_value(state, state.recording, RecordingState::RECORDING,
                                           StateCondition::ONLINE, StateSource::MOCK);
                case CommandType::RECORDING_STOP:
                    return set_state_value(state, state.recording, RecordingState::STOPPED,
                                           StateCondition::ONLINE, StateSource::MOCK);
                case CommandType::RTSP_START:
                    return set_state_value(state, state.rtsp, BinaryState::ON,
                                           StateCondition::ONLINE, StateSource::MOCK);
                case CommandType::RTSP_STOP:
                    return set_state_value(state, state.rtsp, BinaryState::OFF,
                                           StateCondition::ONLINE, StateSource::MOCK);
                case CommandType::MEDIA_PLAY:
                    return set_state_value(state, state.media, MediaState::PLAYING,
                                           StateCondition::ONLINE, StateSource::MOCK);
                case CommandType::MEDIA_PAUSE:
                    return set_state_value(state, state.media, MediaState::PAUSED,
                                           StateCondition::ONLINE, StateSource::MOCK);
                case CommandType::MEDIA_STOP:
                    return set_state_value(state, state.media, MediaState::STOPPED,
                                           StateCondition::ONLINE, StateSource::MOCK);
                case CommandType::VOICE_SESSION_START:
                    return set_state_value(state, state.voice, VoiceState::ACTIVE,
                                           StateCondition::ONLINE, StateSource::MOCK);
                case CommandType::VOICE_SESSION_CANCEL:
                    return set_state_value(state, state.voice, VoiceState::IDLE,
                                           StateCondition::OFFLINE, StateSource::MOCK);
                case CommandType::SIM_LED_SET: {
                    const auto* enabled = parameter(command, "enabled");
                    return set_state_value(state, state.simulated_led,
                        enabled != nullptr && *enabled == "true" ? BinaryState::ON : BinaryState::OFF,
                        StateCondition::SIMULATED, StateSource::MOCK);
                }
                case CommandType::SIM_BUZZER_SET: {
                    const auto* enabled = parameter(command, "enabled");
                    return set_state_value(state, state.simulated_buzzer,
                        enabled != nullptr && *enabled == "true" ? BinaryState::ON : BinaryState::OFF,
                        StateCondition::SIMULATED, StateSource::MOCK);
                }
                case CommandType::QUERY_STATE:
                case CommandType::CAMERA_SNAPSHOT: return false;
            }
            return false;
        });
        if (changed) publish_state();
    }

    void publish_state() {
        const auto state = snapshot();
        std::vector<StateCallback> callbacks;
        {
            std::lock_guard<std::mutex> lock(subscriber_mutex_);
            callbacks = subscribers_;
        }
        for (const auto& callback : callbacks) {
            try { callback(state); } catch (...) {}
        }
    }

    VehicleCoreConfig config_;
    AdapterSet adapters_;
    std::shared_ptr<ServiceRegistry> registry_;
    std::shared_ptr<IClock> clock_;
    StateStore state_store_;
    std::shared_ptr<CompletionPort> completion_port_;
    mutable std::mutex lifecycle_mutex_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopping_{false};
    std::thread worker_;
    std::unique_ptr<ipc::BoundedQueue<ControlEvent>> command_queue_;
    std::unique_ptr<ipc::BoundedQueue<CompletionEvent>> completion_queue_;
    std::uint64_t generation_{0};
    mutable std::mutex request_mutex_;
    std::unordered_map<protocol::RequestId, std::shared_ptr<RequestRecord>> requests_;
    std::deque<protocol::RequestId> completed_order_;
    mutable std::mutex subscriber_mutex_;
    std::vector<StateCallback> subscribers_;
    std::atomic<std::uint64_t> lifecycle_sequence_{0};
    std::atomic<std::uint64_t> ignored_late_results_{0};
};

VehicleCore::VehicleCore(VehicleCoreConfig config, AdapterSet adapters,
                         std::shared_ptr<ServiceRegistry> registry,
                         std::shared_ptr<IClock> clock)
    : impl_(std::make_unique<Impl>(config, std::move(adapters), std::move(registry), std::move(clock))) {}

VehicleCore::~VehicleCore() = default;
protocol::Status VehicleCore::start() { return impl_->start(); }
void VehicleCore::stop() { impl_->stop(); }
bool VehicleCore::running() const { return impl_->running(); }
CommandSubmission VehicleCore::send_command(const VehicleCommand& command) { return impl_->send_command(command); }
VehicleState VehicleCore::get_snapshot() const { return impl_->snapshot(); }
protocol::Status VehicleCore::subscribe_state(StateCallback callback) { return impl_->subscribe(std::move(callback)); }
protocol::Status VehicleCore::set_service_health(ServiceDomain domain, ServiceHealth health, StateSource source) {
    return impl_->set_service_health(domain, health, source);
}
protocol::Status VehicleCore::poll_deadlines() { return impl_->poll_deadlines(); }
protocol::BootEpoch VehicleCore::boot_epoch() const { return impl_->boot_epoch(); }
std::uint64_t VehicleCore::ignored_late_results() const { return impl_->ignored_late_results(); }

}  // namespace cockpit::vehicle
