#pragma once

#include "cockpit/protocol/message.hpp"

#include <cstdint>
#include <future>
#include <string>
#include <utility>
#include <vector>

namespace cockpit::vehicle {

enum class CommandSource : std::uint16_t { UI = 1, VOICE, SYSTEM, REMOTE, TEST };

enum class CommandType : std::uint16_t {
    QUERY_STATE = 1,
    CAMERA_SELECT,
    CAMERA_SNAPSHOT,
    RECORDING_START,
    RECORDING_STOP,
    RTSP_START,
    RTSP_STOP,
    MEDIA_PLAY,
    MEDIA_PAUSE,
    MEDIA_STOP,
    VOICE_SESSION_START,
    VOICE_SESSION_CANCEL,
    SIM_LED_SET,
    SIM_BUZZER_SET,
};

using CommandParameters = std::vector<std::pair<std::string, std::string>>;

struct VehicleCommand {
    std::uint16_t protocol_version{protocol::kProtocolVersion};
    protocol::MessageType message_type{protocol::MessageType::VEHICLE_COMMAND};
    protocol::RequestId request_id{0};
    protocol::SessionId session_id{0};
    protocol::BootEpoch boot_epoch{0};
    protocol::Deadline deadline_ms{0};
    std::uint64_t voice_generation{0};
    std::uint64_t asr_sequence{0};
    CommandSource source{CommandSource::SYSTEM};
    CommandType command_type{CommandType::QUERY_STATE};
    CommandParameters parameters;
};

struct CommandAck {
    protocol::RequestId request_id{0};
    protocol::SessionId session_id{0};
    protocol::BootEpoch boot_epoch{0};
    std::uint64_t lifecycle_sequence{0};
    protocol::Status status;
};

struct CommandResult {
    protocol::RequestId request_id{0};
    protocol::SessionId session_id{0};
    protocol::BootEpoch boot_epoch{0};
    std::uint64_t lifecycle_sequence{0};
    protocol::Status status;
    bool simulated{false};
};

enum class SubmissionDisposition { REJECTED, ACCEPTED, REPLAYED };

struct CommandSubmission {
    protocol::Status status;
    SubmissionDisposition disposition{SubmissionDisposition::REJECTED};
    bool ack_emitted{false};
    CommandAck ack;
    std::shared_future<CommandResult> result;

    bool accepted() const {
        return status.ok() && disposition == SubmissionDisposition::ACCEPTED && ack_emitted;
    }
    bool replayed() const { return status.ok() && disposition == SubmissionDisposition::REPLAYED; }
};

struct CommandDecodeResult {
    protocol::Status status;
    VehicleCommand command;
};

protocol::Status validate_command_shape(const VehicleCommand& command,
                                        protocol::BootEpoch current_epoch,
                                        protocol::Deadline now_ms);
protocol::Status encode_command_message(const VehicleCommand& command, protocol::Message& message);
CommandDecodeResult decode_command_message(const protocol::Message& message);
std::string command_fingerprint(const VehicleCommand& command);

}  // namespace cockpit::vehicle
