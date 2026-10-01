#include "cockpit/protocol/message.hpp"

#include <limits>

namespace cockpit::protocol {
namespace {
constexpr std::uint32_t kMagic = 0x56414931U;  // VAI1

void put(std::vector<std::uint8_t>& out, std::uint64_t value, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i) {
        const unsigned shift = (bytes - 1 - i) * 8;
        out.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

std::uint64_t get(const std::vector<std::uint8_t>& in, std::size_t& pos, unsigned bytes) {
    std::uint64_t value = 0;
    for (unsigned i = 0; i < bytes; ++i) value = (value << 8) | in[pos++];
    return value;
}
}  // namespace

bool is_known_type(MessageType type) {
    const auto value = static_cast<std::uint16_t>(type);
    return value >= static_cast<std::uint16_t>(MessageType::SERVICE_HELLO) &&
           value <= static_cast<std::uint16_t>(MessageType::STATE_CHANGED);
}

bool is_request_type(MessageType type) {
    switch (type) {
        case MessageType::AUDIO_CAPTURE_START:
        case MessageType::AUDIO_CAPTURE_STOP:
        case MessageType::ASR_START:
        case MessageType::INTENT_REQUEST:
        case MessageType::LLM_REQUEST:
        case MessageType::TTS_REQUEST:
        case MessageType::VEHICLE_COMMAND: return true;
        default: return false;
    }
}

Status encode(const Message& message, std::vector<std::uint8_t>& wire) {
    const auto& h = message.header;
    if (h.protocol_version != kProtocolVersion) return {StatusCode::UNSUPPORTED_VERSION, "version"};
    if (!is_known_type(h.message_type)) return {StatusCode::MALFORMED, "message type"};
    if (message.payload.size() > MAX_CONTROL_MESSAGE_SIZE - kWireHeaderSize ||
        h.payload_size != message.payload.size()) return {StatusCode::INVALID_ARGUMENT, "payload size"};
    std::vector<std::uint8_t> out;
    out.reserve(kWireHeaderSize + message.payload.size());
    put(out, kMagic, 4);
    put(out, h.protocol_version, 2);
    put(out, static_cast<std::uint16_t>(h.message_type), 2);
    put(out, h.payload_size, 4);
    put(out, h.request_id, 8);
    put(out, h.session_id, 8);
    put(out, h.boot_epoch, 8);
    put(out, h.deadline_ms, 8);
    out.insert(out.end(), message.payload.begin(), message.payload.end());
    wire = std::move(out);
    return Status::Ok();
}

DecodeResult decode(const std::vector<std::uint8_t>& wire) {
    if (wire.size() < kWireHeaderSize || wire.size() > MAX_CONTROL_MESSAGE_SIZE)
        return {{StatusCode::MALFORMED, "wire length"}, {}};
    std::size_t pos = 0;
    if (get(wire, pos, 4) != kMagic) return {{StatusCode::MALFORMED, "magic"}, {}};
    Message message;
    auto& h = message.header;
    h.protocol_version = static_cast<std::uint16_t>(get(wire, pos, 2));
    if (h.protocol_version != kProtocolVersion)
        return {{StatusCode::UNSUPPORTED_VERSION, "version"}, {}};
    h.message_type = static_cast<MessageType>(get(wire, pos, 2));
    if (!is_known_type(h.message_type)) return {{StatusCode::MALFORMED, "message type"}, {}};
    h.payload_size = static_cast<std::uint32_t>(get(wire, pos, 4));
    if (h.payload_size > MAX_CONTROL_MESSAGE_SIZE - kWireHeaderSize ||
        h.payload_size != wire.size() - kWireHeaderSize)
        return {{StatusCode::MALFORMED, "payload length"}, {}};
    h.request_id = get(wire, pos, 8);
    h.session_id = get(wire, pos, 8);
    h.boot_epoch = get(wire, pos, 8);
    h.deadline_ms = get(wire, pos, 8);
    message.payload.assign(wire.begin() + static_cast<std::ptrdiff_t>(pos), wire.end());
    return {Status::Ok(), std::move(message)};
}

RequestFence::RequestFence(BootEpoch epoch, SessionId session, std::size_t recent_capacity)
    : epoch_(epoch), session_(session), capacity_(recent_capacity) {}

void RequestFence::set_session(SessionId session) {
    session_ = session;
    recent_ids_.clear();
    recent_set_.clear();
}

Status RequestFence::accept(const Message& message, Deadline now_ms) {
    const auto& h = message.header;
    if (h.boot_epoch != epoch_) return {StatusCode::STALE_EPOCH, "boot epoch"};
    if (h.session_id != session_) return {StatusCode::STALE_SESSION, "session"};
    if (h.deadline_ms != 0 && now_ms > h.deadline_ms) return {StatusCode::EXPIRED, "deadline"};
    if (is_request_type(h.message_type)) {
        if (h.request_id == 0 || capacity_ == 0)
            return {StatusCode::INVALID_ARGUMENT, "request id or fence capacity"};
        if (recent_set_.count(h.request_id) != 0)
            return {StatusCode::DUPLICATE_REQUEST, "request id"};
        if (recent_ids_.size() == capacity_) {
            recent_set_.erase(recent_ids_.front());
            recent_ids_.pop_front();
        }
        recent_ids_.push_back(h.request_id);
        recent_set_.insert(h.request_id);
    }
    return Status::Ok();
}

}  // namespace cockpit::protocol
