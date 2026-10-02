#include "cockpit/media/fake_camera_capture.hpp"
#include "cockpit/media/fake_h264_encoder.hpp"
#include "cockpit/media/file_recording_sink.hpp"
#include "cockpit/media/media_service.hpp"
#include "cockpit/media/real_media_service_adapter.hpp"
#include "cockpit/vehicle/client.hpp"
#include "cockpit/vehicle/clock.hpp"
#include "cockpit/vehicle/core.hpp"
#include "cockpit/vehicle/service_adapter.hpp"
#include "cockpit/vehicle/service_registry.hpp"
#include "cockpit/vehicle/voice_command_sink.hpp"
#include "cockpit/voice/intent_dispatcher.hpp"

#ifdef COCKPIT_ENABLE_V4L2_CAMERA
#include "cockpit/media/v4l2_mplane_camera_capture.hpp"
#endif
#ifdef COCKPIT_ENABLE_MPP_RECORDING
#include "cockpit/media/mpp_h264_encoder.hpp"
#endif

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace std::chrono_literals;
using namespace cockpit;

struct Options {
    std::string backend{"fake"};
    std::string device;
    std::string text;
    std::string snapshot_directory{"/tmp/cockpit-voice-media-snapshots"};
    std::string recording_directory{"/tmp/cockpit-voice-media-recordings"};
    bool suite{false};
    bool duplicate_final{false};
};

Options parse_options(int argc, char* argv[]) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument(argv[index]);
        const auto take = [&](const char* name) -> std::string {
            if (++index >= argc) throw std::runtime_error(std::string("missing value for ") + name);
            return argv[index];
        };
        if (argument == "--backend") options.backend = take("--backend");
        else if (argument == "--device") options.device = take("--device");
        else if (argument == "--text") options.text = take("--text");
        else if (argument == "--snapshot-dir")
            options.snapshot_directory = take("--snapshot-dir");
        else if (argument == "--recording-dir")
            options.recording_directory = take("--recording-dir");
        else if (argument == "--suite") options.suite = true;
        else if (argument == "--duplicate-final") options.duplicate_final = true;
        else throw std::runtime_error("unknown argument: " + argument);
    }
    if (options.backend != "fake" && options.backend != "cam0")
        throw std::runtime_error("--backend must be fake|cam0");
    if (options.backend == "cam0" && options.device.empty())
        throw std::runtime_error("--device is required for cam0; pass the runtime-resolved node");
    if (!options.suite && options.text.empty())
        throw std::runtime_error("provide --suite or --text");
    return options;
}

void require(bool condition, const std::string& detail) {
    if (!condition) throw std::runtime_error(detail);
}

template <typename Predicate>
bool wait_until(Predicate predicate, std::chrono::milliseconds timeout = 2s) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) return true;
        std::this_thread::sleep_for(1ms);
    }
    return predicate();
}

const char* command_name(vehicle::CommandType type) {
    switch (type) {
    case vehicle::CommandType::CAMERA_SELECT: return "CAMERA_SELECT";
    case vehicle::CommandType::CAMERA_PREVIEW_START: return "CAMERA_PREVIEW_START";
    case vehicle::CommandType::CAMERA_PREVIEW_STOP: return "CAMERA_PREVIEW_STOP";
    case vehicle::CommandType::RECORDING_START: return "RECORDING_START";
    case vehicle::CommandType::RECORDING_STOP: return "RECORDING_STOP";
    default: return "OTHER";
    }
}

const char* recording_name(vehicle::RecordingState state) {
    switch (state) {
    case vehicle::RecordingState::STOPPED: return "STOPPED";
    case vehicle::RecordingState::STARTING: return "STARTING";
    case vehicle::RecordingState::RECORDING: return "RECORDING";
    case vehicle::RecordingState::STOPPING: return "STOPPING";
    case vehicle::RecordingState::ERROR: return "ERROR";
    }
    return "UNKNOWN";
}

const char* preview_name(vehicle::PreviewState state) {
    switch (state) {
    case vehicle::PreviewState::STOPPED: return "STOPPED";
    case vehicle::PreviewState::STARTING: return "STARTING";
    case vehicle::PreviewState::STREAMING: return "STREAMING";
    case vehicle::PreviewState::STOPPING: return "STOPPING";
    case vehicle::PreviewState::ERROR: return "ERROR";
    }
    return "UNKNOWN";
}

const char* source_name(vehicle::StateSource source) {
    switch (source) {
    case vehicle::StateSource::UNKNOWN: return "UNKNOWN";
    case vehicle::StateSource::RUNTIME: return "RUNTIME";
    case vehicle::StateSource::HISTORICAL: return "HISTORICAL";
    case vehicle::StateSource::MOCK: return "MOCK";
    }
    return "UNKNOWN";
}

class CapturingClient final : public vehicle::IVehicleCoreClient {
public:
    explicit CapturingClient(vehicle::IVehicleCoreClient& inner) : inner_(inner) {}

    vehicle::CommandSubmission send_command(const vehicle::VehicleCommand& command) override {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            last_ = command;
            ++count_;
            ++counts_[command.command_type];
        }
        return inner_.send_command(command);
    }
    vehicle::VehicleState get_snapshot() const override { return inner_.get_snapshot(); }
    protocol::Status subscribe_state(vehicle::StateCallback callback) override {
        return inner_.subscribe_state(std::move(callback));
    }
    protocol::BootEpoch boot_epoch() const override { return inner_.boot_epoch(); }

    std::size_t count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }
    std::size_t count(vehicle::CommandType type) const {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto found = counts_.find(type);
        return found == counts_.end() ? 0 : found->second;
    }
    std::optional<vehicle::VehicleCommand> last() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_;
    }

private:
    vehicle::IVehicleCoreClient& inner_;
    mutable std::mutex mutex_;
    std::map<vehicle::CommandType, std::size_t> counts_;
    std::optional<vehicle::VehicleCommand> last_;
    std::size_t count_{0};
};

class Reports {
public:
    void add(const voice::IntentDispatchReport& report) {
        std::lock_guard<std::mutex> lock(mutex_);
        values_.push_back(report);
        ready_.notify_all();
    }
    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return values_.size();
    }
    bool wait(std::size_t count, std::chrono::milliseconds timeout = 3s) {
        std::unique_lock<std::mutex> lock(mutex_);
        return ready_.wait_for(lock, timeout, [&] { return values_.size() >= count; });
    }
    std::vector<voice::IntentDispatchReport> slice(std::size_t start, std::size_t count) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (start + count > values_.size()) return {};
        return {values_.begin() + static_cast<std::ptrdiff_t>(start),
                values_.begin() + static_cast<std::ptrdiff_t>(start + count)};
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::vector<voice::IntentDispatchReport> values_;
};

struct DispatchEvidence {
    std::vector<voice::IntentDispatchReport> reports;
    std::optional<vehicle::VehicleCommand> command;
    std::optional<vehicle::CommandSubmission> submission;
    std::optional<vehicle::CommandResult> result;
    vehicle::VehicleState state;
    std::size_t command_count_before{0};
    std::size_t command_count_after{0};
};

media::MediaServiceConfig media_config(const Options& options) {
    media::MediaServiceConfig config;
    config.capture.device = options.backend == "fake" ? "fake-cam0" : options.device;
    config.capture.camera_id = "front";
    config.capture.width = options.backend == "fake" ? 8 : 1632;
    config.capture.height = options.backend == "fake" ? 4 : 1224;
    config.capture.pixel_format = "NV12";
    config.capture.fps = 30;
    config.capture.buffer_count = 4;
    config.capture.poll_timeout_ms = 200;
    config.snapshot_directory = options.snapshot_directory;
    config.recording_directory = options.recording_directory;
    config.snapshot_wait = 2s;
    return config;
}

class IntegrationHarness {
public:
    IntegrationHarness(media::MediaServiceConfig config,
                       std::unique_ptr<media::ICameraCapture> capture,
                       std::unique_ptr<media::IH264Encoder> encoder,
                       std::shared_ptr<vehicle::IClock> clock,
                       protocol::BootEpoch epoch = 20261002)
        : service(std::make_shared<media::MediaService>(
              std::move(config), std::move(capture),
              std::make_shared<media::PreviewMailbox>(), std::move(encoder),
              std::make_unique<media::FileRecordingSink>())),
          media_adapter(std::make_shared<media::RealMediaServiceAdapter>(service)),
          voice_adapter(std::make_shared<vehicle::MockVoiceAdapter>()),
          rtos_adapter(std::make_shared<vehicle::MockRtosAdapter>()),
          system_adapter(std::make_shared<vehicle::MockSystemAdapter>()),
          registry(std::make_shared<vehicle::ServiceRegistry>()), clock(std::move(clock)),
          core({epoch, 16, 16, 128, 2ms},
               {media_adapter, voice_adapter, rtos_adapter, system_adapter}, registry, this->clock),
          inner_client(core), client(inner_client), controller(epoch),
          sink(client, this->clock, epoch),
          dispatcher(router, controller, sink, [this] { return this->clock->now_ms(); },
                     [this](const voice::IntentDispatchReport& report) { reports.add(report); },
                     16, 64) {
        registry->set(vehicle::ServiceDomain::MEDIA, vehicle::ServiceHealth::ONLINE,
                      vehicle::StateSource::RUNTIME);
        registry->set(vehicle::ServiceDomain::VOICE, vehicle::ServiceHealth::ONLINE,
                      vehicle::StateSource::MOCK);
        registry->set(vehicle::ServiceDomain::RTOS, vehicle::ServiceHealth::ONLINE,
                      vehicle::StateSource::MOCK);
        registry->set(vehicle::ServiceDomain::SYSTEM, vehicle::ServiceHealth::ONLINE,
                      vehicle::StateSource::MOCK);
        media_adapter->set_runtime_state_callback(
            [this](vehicle::CommandType type, vehicle::AdapterResult result) {
                (void)core.report_runtime_result(type, std::move(result));
            });
    }

    ~IntegrationHarness() { stop(); }

    void start() {
        require(service->start().ok(), "media service start");
        require(core.start().ok(), "vehicle core start");
        require(dispatcher.start().ok(), "intent dispatcher start");
        started_ = true;
    }

    void stop() {
        if (!started_) return;
        dispatcher.stop();
        core.stop();
        service->stop();
        started_ = false;
    }

    DispatchEvidence final(const std::string& text, bool duplicate = false,
                           bool wait_for_result = true) {
        const auto report_start = reports.size();
        DispatchEvidence evidence;
        evidence.command_count_before = client.count();
        const auto token = controller.start(next_request_id_++);
        require(token.session_id != 0, "voice session start");
        require(controller.transition(token, voice::VoiceSessionState::Recognizing).ok(),
                "voice recognizing transition");
        const auto deadline = clock->now_ms() + 5000;
        require(sink.activate_session(token, deadline).ok(), "voice sink session activation");
        const voice::AsrEvent event{voice::AsrEventType::FINAL, token, next_asr_sequence_++,
                                    text, protocol::Status::Ok()};
        protocol::Status first;
        protocol::Status second;
        require(controller.deliver_event(token, protocol::MessageType::ASR_FINAL, [&] {
                    first = dispatcher.enqueue_event(event, deadline);
                    if (duplicate) second = dispatcher.enqueue_event(event, deadline);
                }).ok(), "ASR_FINAL delivery");
        require(first.ok() && (!duplicate || second.ok()), "intent enqueue");
        const std::size_t expected = duplicate ? 2 : 1;
        require(reports.wait(report_start + expected), "intent report timeout");
        evidence.reports = reports.slice(report_start, expected);
        evidence.command_count_after = client.count();
        evidence.command = client.last();
        evidence.submission = sink.last_submission();
        if (evidence.submission && evidence.submission->accepted() && wait_for_result) {
            require(evidence.submission->result.wait_for(10s) == std::future_status::ready,
                    "Vehicle RESULT timeout");
            evidence.result = evidence.submission->result.get();
        }
        evidence.state = client.get_snapshot();
        return evidence;
    }

    void partial(const std::string& text) {
        const auto before_reports = reports.size();
        const auto before_commands = client.count();
        const auto token = controller.start(next_request_id_++);
        require(controller.transition(token, voice::VoiceSessionState::Recognizing).ok(),
                "partial recognizing transition");
        const auto deadline = clock->now_ms() + 5000;
        require(sink.activate_session(token, deadline).ok(), "partial sink activation");
        const voice::AsrEvent event{voice::AsrEventType::PARTIAL, token, next_asr_sequence_++,
                                    text, protocol::Status::Ok()};
        require(controller.deliver_event(token, protocol::MessageType::ASR_PARTIAL, [&] {
                    require(dispatcher.enqueue_event(event, deadline).ok(), "partial enqueue");
                }).ok(), "ASR_PARTIAL delivery");
        std::this_thread::sleep_for(100ms);
        require(reports.size() == before_reports && client.count() == before_commands,
                "PARTIAL reached Core");
    }

    std::shared_ptr<media::MediaService> service;
    std::shared_ptr<media::RealMediaServiceAdapter> media_adapter;
    std::shared_ptr<vehicle::MockVoiceAdapter> voice_adapter;
    std::shared_ptr<vehicle::MockRtosAdapter> rtos_adapter;
    std::shared_ptr<vehicle::MockSystemAdapter> system_adapter;
    std::shared_ptr<vehicle::ServiceRegistry> registry;
    std::shared_ptr<vehicle::IClock> clock;
    vehicle::VehicleCore core;
    vehicle::InProcessVehicleCoreClient inner_client;
    CapturingClient client;
    voice::DeterministicIntentRouter router;
    voice::VoiceSessionController controller;
    vehicle::VehicleCommandSinkAdapter sink;
    Reports reports;
    voice::VoiceIntentDispatcher dispatcher;

private:
    protocol::RequestId next_request_id_{1};
    std::uint64_t next_asr_sequence_{1};
    bool started_{false};
};

void require_success(const DispatchEvidence& evidence, voice::ActionType action,
                     vehicle::CommandType command, vehicle::PreviewState state) {
    require(!evidence.reports.empty(), "missing intent report");
    require(evidence.reports.front().outcome == voice::IntentOutcome::MATCH,
            "intent did not MATCH");
    require(evidence.reports.front().submitted, "intent was not submitted");
    require(evidence.command.has_value() && evidence.command->command_type == command,
            "wrong VehicleCommand mapping");
    require(evidence.command->parameters.empty(), "ASR text or unexpected parameter reached Core");
    require(evidence.submission.has_value() && evidence.submission->accepted() &&
                evidence.submission->ack_emitted,
            "missing ACK");
    require(evidence.result.has_value() && evidence.result->status.ok(), "RESULT failure");
    require(evidence.submission->ack.lifecycle_sequence < evidence.result->lifecycle_sequence,
            "ACK and RESULT lifecycle ordering");
    require(evidence.state.preview.value == state &&
                evidence.state.preview.source == vehicle::StateSource::RUNTIME,
            "canonical preview state/source");
    std::cout << "action=" << voice::action_type_name(action)
              << " command=" << command_name(command)
              << " ack=" << evidence.submission->ack.lifecycle_sequence
              << " result=" << evidence.result->lifecycle_sequence
              << " revision=" << evidence.state.revision
              << " preview=" << preview_name(evidence.state.preview.value)
              << " source=" << source_name(evidence.state.preview.source) << '\n';
}

void run_fake_timeout_late(const Options& options) {
    media::FakeCameraCaptureOptions capture_options;
    capture_options.hold_start = true;
    auto capture = std::make_unique<media::FakeCameraCapture>(capture_options);
    auto* capture_pointer = capture.get();
    auto clock = std::make_shared<vehicle::FakeClock>(1000);
    IntegrationHarness harness(media_config(options), std::move(capture),
                               std::make_unique<media::FakeH264Encoder>(),
                               clock, 20261003);
    harness.start();
    const auto pending = harness.final("打开摄像头", false, false);
    require(pending.submission && pending.submission->accepted(), "timeout ACK missing");
    require(capture_pointer->wait_until_start_entered(2s), "held preview start not entered");
    clock->advance(5001);
    require(harness.core.poll_deadlines().ok(), "deadline poll");
    require(pending.submission->result.wait_for(2s) == std::future_status::ready,
            "timeout RESULT missing");
    require(pending.submission->result.get().status.code == protocol::StatusCode::TIMEOUT,
            "timeout status");
    const auto timeout_state = harness.client.get_snapshot();
    require(timeout_state.preview.value == vehicle::PreviewState::ERROR,
            "timeout canonical state");
    const auto revision = timeout_state.revision;
    capture_pointer->release_start();
    require(wait_until([&] { return harness.core.ignored_late_results() >= 1; }),
            "late RESULT not fenced");
    require(harness.client.get_snapshot().revision == revision &&
                harness.client.get_snapshot().preview.value == vehicle::PreviewState::ERROR,
            "late success changed canonical state");
    std::cout << "timeout_late_result=PASS revision=" << revision
              << " ignored=" << harness.core.ignored_late_results() << '\n';
    harness.stop();
}

void run_suite(IntegrationHarness& harness, bool host_fake) {
    const auto open = harness.final("打开摄像头");
    require_success(open, voice::ActionType::OPEN_CAMERA,
                    vehicle::CommandType::CAMERA_PREVIEW_START,
                    vehicle::PreviewState::STREAMING);
    require(wait_until([&] { return harness.service->capture_stats().frames >= 3; }, 5s),
            "CAM0 produced no frames after STREAMON");
    std::cout << "T1=PASS frames=" << harness.service->capture_stats().frames
              << " epoch=" << harness.service->capture_stats().stream_epoch << '\n';

    const auto close = harness.final("关闭摄像头");
    require_success(close, voice::ActionType::CLOSE_CAMERA,
                    vehicle::CommandType::CAMERA_PREVIEW_STOP,
                    vehicle::PreviewState::STOPPED);
    require(!harness.service->streaming(), "CAM0 still streaming after close intent");
    std::cout << "T2=PASS\n";

    const auto starts_before = harness.service->service_stats().preview_start_requests;
    const auto commands_before = harness.client.count(vehicle::CommandType::CAMERA_PREVIEW_START);
    const auto reopen_duplicate = harness.final("打开摄像头", true);
    require_success(reopen_duplicate, voice::ActionType::OPEN_CAMERA,
                    vehicle::CommandType::CAMERA_PREVIEW_START,
                    vehicle::PreviewState::STREAMING);
    require(reopen_duplicate.reports.size() == 2 && reopen_duplicate.reports[1].duplicate,
            "duplicate FINAL was not rejected");
    require(harness.client.count(vehicle::CommandType::CAMERA_PREVIEW_START) == commands_before + 1,
            "duplicate FINAL reached Core");
    require(harness.service->service_stats().preview_start_requests == starts_before + 1,
            "duplicate FINAL reached MediaService");
    require(wait_until([&] { return harness.service->capture_stats().frames >= 3; }, 5s),
            "CAM0 restart produced no frames");
    std::cout << "T3=PASS epoch=" << harness.service->capture_stats().stream_epoch << '\n'
              << "T4=PASS duplicate_final_service_calls=1\n";

    const auto rear = harness.final("切换后摄");
    require(!rear.reports.empty() && rear.reports.front().outcome == voice::IntentOutcome::MATCH &&
                rear.submission && rear.submission->accepted() && rear.result &&
                rear.result->status.code == protocol::StatusCode::UNAVAILABLE,
            "rear unavailable RESULT");
    require(rear.state.selected_camera.value == vehicle::CameraSelection::FRONT &&
                rear.state.preview.value == vehicle::PreviewState::STREAMING,
            "rear request damaged CAM0 state");
    std::cout << "T5=PASS rear=UNAVAILABLE selected=FRONT preview=STREAMING\n";

    const auto commands_before_negation = harness.client.count();
    const auto starts_before_negation = harness.service->service_stats().preview_start_requests;
    const auto negated = harness.final("不要打开摄像头");
    require(!negated.reports.empty() &&
                negated.reports.front().outcome == voice::IntentOutcome::REJECTED_NEGATED &&
                !negated.reports.front().submitted,
            "negation was not rejected");
    require(harness.client.count() == commands_before_negation &&
                harness.service->service_stats().preview_start_requests == starts_before_negation,
            "negation caused hardware action");
    std::cout << "T6=PASS negation_core_commands=0 hardware_actions=0\n";

    if (host_fake) {
        const auto commands_before_no_match = harness.client.count();
        const auto no_match = harness.final("今天天气怎么样");
        require(!no_match.reports.empty() &&
                    no_match.reports.front().outcome == voice::IntentOutcome::NO_MATCH &&
                    harness.client.count() == commands_before_no_match,
                "NO_MATCH reached Core");
        harness.partial("打开摄像头");
        std::cout << "host_partial_no_match=PASS core_commands=0\n";
    }

    const auto recording_starts = harness.service->service_stats().recording_start_requests;
    const auto record_start = harness.final("开始录像", true);
    require(!record_start.reports.empty() &&
                record_start.reports.front().outcome == voice::IntentOutcome::MATCH &&
                record_start.command &&
                record_start.command->command_type == vehicle::CommandType::RECORDING_START &&
                record_start.command->parameters.empty() &&
                record_start.submission && record_start.submission->accepted() &&
                record_start.result && record_start.result->status.ok() &&
                record_start.state.recording.value == vehicle::RecordingState::RECORDING &&
                record_start.state.recording.source == vehicle::StateSource::RUNTIME,
            "recording start intent path");
    require(record_start.reports.size() == 2 && record_start.reports[1].duplicate,
            "duplicate recording FINAL was not rejected");
    require(harness.service->service_stats().recording_start_requests ==
                recording_starts + 1,
            "duplicate recording FINAL reached MediaService");
    std::cout << "record_start_intent=PASS ack="
              << record_start.submission->ack.lifecycle_sequence
              << " result=" << record_start.result->lifecycle_sequence
              << " recording=" << recording_name(record_start.state.recording.value)
              << " source=" << source_name(record_start.state.recording.source) << '\n';

    const auto record_stop = harness.final("停止录像");
    require(record_stop.command &&
                record_stop.command->command_type == vehicle::CommandType::RECORDING_STOP &&
                record_stop.submission && record_stop.submission->accepted() &&
                record_stop.result && record_stop.result->status.ok() &&
                record_stop.state.recording.value == vehicle::RecordingState::STOPPED &&
                harness.service->recorder_stats().file_closed &&
                harness.service->preview_active(),
            "recording stop intent path");
    std::cout << "record_stop_intent=PASS ack="
              << record_stop.submission->ack.lifecycle_sequence
              << " result=" << record_stop.result->lifecycle_sequence
              << " recording=" << recording_name(record_stop.state.recording.value)
              << " preview=" << preview_name(record_stop.state.preview.value) << '\n';

    const auto cleanup = harness.final("关闭摄像头");
    require_success(cleanup, voice::ActionType::CLOSE_CAMERA,
                    vehicle::CommandType::CAMERA_PREVIEW_STOP,
                    vehicle::PreviewState::STOPPED);
    require(!harness.service->streaming(), "cleanup preview stop");
    std::cout << "recording=RUNTIME rtsp=UNAVAILABLE rear=UNAVAILABLE\n";
}

void run_single(IntegrationHarness& harness, const Options& options) {
    const auto evidence = harness.final(options.text, options.duplicate_final);
    const auto& report = evidence.reports.front();
    std::cout << "intent=" << voice::intent_outcome_name(report.outcome)
              << " submitted=" << (report.submitted ? "true" : "false")
              << " duplicate=" << (report.duplicate ? "true" : "false") << '\n';
    if (evidence.command)
        std::cout << "command=" << command_name(evidence.command->command_type)
                  << " parameter_count=" << evidence.command->parameters.size() << '\n';
    if (evidence.submission && evidence.submission->ack_emitted)
        std::cout << "ack=" << evidence.submission->ack.lifecycle_sequence << '\n';
    if (evidence.result)
        std::cout << "result=" << evidence.result->lifecycle_sequence
                  << " status=" << static_cast<int>(evidence.result->status.code) << '\n';
    std::cout << "revision=" << evidence.state.revision
              << " preview=" << preview_name(evidence.state.preview.value)
              << " recording=" << recording_name(evidence.state.recording.value)
              << " source=" << source_name(evidence.state.preview.source) << '\n';
}

std::unique_ptr<media::IH264Encoder> make_encoder(const Options& options) {
    if (options.backend == "fake")
        return std::make_unique<media::FakeH264Encoder>();
#ifdef COCKPIT_ENABLE_MPP_RECORDING
    return std::make_unique<media::MppH264Encoder>();
#else
    throw std::runtime_error(
        "cam0 recording backend not built; configure COCKPIT_ENABLE_MPP_RECORDING=ON");
#endif
}

std::unique_ptr<media::ICameraCapture> make_capture(const Options& options) {
    if (options.backend == "fake") return std::make_unique<media::FakeCameraCapture>();
#ifdef COCKPIT_ENABLE_V4L2_CAMERA
    return std::make_unique<media::V4l2MplaneCameraCapture>();
#else
    throw std::runtime_error("cam0 backend not built; configure COCKPIT_ENABLE_V4L2_CAMERA=ON");
#endif
}

#ifdef COCKPIT_ENABLE_V4L2_CAMERA
void verify_reopen(const Options& options) {
    if (options.backend != "cam0") return;
    media::V4l2MplaneCameraCapture capture;
    const auto config = media_config(options).capture;
    require(capture.open_device(options.device).ok(), "post-suite CAM0 reopen");
    require(capture.configure(config).ok(), "post-suite CAM0 reconfigure");
    capture.close_device();
    std::cout << "camera_reopen=PASS\n";
}
#else
void verify_reopen(const Options&) {}
#endif

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const auto options = parse_options(argc, argv);
        std::cout << "backend=" << options.backend << '\n';
        if (!options.device.empty()) std::cout << "device=" << options.device << '\n';
        auto clock = std::make_shared<vehicle::SystemClock>();
        IntegrationHarness harness(media_config(options), make_capture(options),
                                   make_encoder(options), clock);
        harness.start();
        if (options.suite) run_suite(harness, options.backend == "fake");
        else run_single(harness, options);
        harness.stop();
        require(!harness.service->streaming(), "service still streaming after shutdown");
        verify_reopen(options);
        if (options.backend == "fake" && options.suite) run_fake_timeout_late(options);
        std::cout << "VOICE_INTENT_REAL_MEDIA_TEST_PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "cockpit_voice_media_test: FAIL: " << error.what() << '\n';
        return 1;
    }
}
