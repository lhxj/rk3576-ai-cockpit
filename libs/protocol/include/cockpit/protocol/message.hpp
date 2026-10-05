#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <unordered_set>
#include <vector>

namespace cockpit::protocol {

constexpr std::uint16_t kProtocolVersion = 1;
constexpr std::size_t kWireHeaderSize = 44;
constexpr std::size_t MAX_CONTROL_MESSAGE_SIZE = 64 * 1024;
using RequestId = std::uint64_t;
using SessionId = std::uint64_t;
using BootEpoch = std::uint64_t;
using Deadline = std::uint64_t;  // Unix milliseconds; same Linux host clock domain.

enum class StatusCode {
    OK,
    INVALID_ARGUMENT,
    INVALID_STATE,
    TIMEOUT,
    CANCELLED,
    UNAVAILABLE,
    INTERNAL_ERROR,
    MALFORMED,
    UNSUPPORTED_VERSION,
    DUPLICATE_REQUEST,
    STALE_SESSION,
    STALE_EPOCH,
    EXPIRED,
    UNSUPPORTED_ACTION,
};

struct Status {
    StatusCode code{StatusCode::OK};
    std::string detail;
    bool ok() const { return code == StatusCode::OK; }
    static Status Ok() { return {}; }
};

enum class MessageType : std::uint16_t {
    SERVICE_HELLO = 1, SERVICE_READY, SERVICE_HEARTBEAT,
    AUDIO_CAPTURE_START, AUDIO_CAPTURE_STOP,
    ASR_START, ASR_PARTIAL, ASR_FINAL, ASR_ERROR,
    INTENT_REQUEST, INTENT_RESULT,
    LLM_REQUEST, LLM_CHUNK, LLM_RESULT,
    TTS_REQUEST, TTS_STARTED, TTS_FINISHED,
    CANCEL, ACK, RESULT, ERROR,
    VEHICLE_COMMAND, STATE_SNAPSHOT, STATE_CHANGED,
    SENSOR_HELLO = 0x100, SENSOR_STATUS, SENSOR_SUBSCRIBE, SENSOR_UNSUBSCRIBE, SENSOR_SAMPLE,
};

struct MessageHeader {
    std::uint16_t protocol_version{kProtocolVersion};
    MessageType message_type{MessageType::SERVICE_HELLO};
    std::uint32_t payload_size{0};
    RequestId request_id{0};
    SessionId session_id{0};
    BootEpoch boot_epoch{0};
    Deadline deadline_ms{0};
};

struct Message {
    MessageHeader header;
    std::vector<std::uint8_t> payload;
};

struct DecodeResult {
    Status status;
    Message message;
};

Status encode(const Message& message, std::vector<std::uint8_t>& wire);
DecodeResult decode(const std::vector<std::uint8_t>& wire);
bool is_known_type(MessageType type);
bool is_request_type(MessageType type);

// Stateful receive-side fence. One instance belongs to one service/session context.
// Only initiating request types consume duplicate-tracker entries; chunks/results may
// share the request id. The bounded history is scoped by the owner lifetime.
class RequestFence {
public:
    RequestFence(BootEpoch epoch, SessionId session, std::size_t recent_capacity);
    Status accept(const Message& message, Deadline now_ms);
    void set_session(SessionId session);
    std::size_t recent_size() const { return recent_ids_.size(); }

private:
    BootEpoch epoch_;
    SessionId session_;
    std::size_t capacity_;
    std::deque<RequestId> recent_ids_;
    std::unordered_set<RequestId> recent_set_;
};

}  // namespace cockpit::protocol
