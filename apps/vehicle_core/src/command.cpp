#include "cockpit/vehicle/command.hpp"

#include <algorithm>
#include <limits>
#include <set>
#include <sstream>

namespace cockpit::vehicle {
namespace {

bool known_source(CommandSource source) {
    switch (source) {
        case CommandSource::UI:
        case CommandSource::VOICE:
        case CommandSource::SYSTEM:
        case CommandSource::REMOTE:
        case CommandSource::TEST: return true;
    }
    return false;
}

bool known_command(CommandType type) {
    switch (type) {
        case CommandType::QUERY_STATE:
        case CommandType::CAMERA_SELECT:
        case CommandType::CAMERA_SNAPSHOT:
        case CommandType::RECORDING_START:
        case CommandType::RECORDING_STOP:
        case CommandType::RTSP_START:
        case CommandType::RTSP_STOP:
        case CommandType::MEDIA_PLAY:
        case CommandType::MEDIA_PAUSE:
        case CommandType::MEDIA_STOP:
        case CommandType::VOICE_SESSION_START:
        case CommandType::VOICE_SESSION_CANCEL:
        case CommandType::SIM_LED_SET:
        case CommandType::SIM_BUZZER_SET:
        case CommandType::MEDIA_PREVIOUS:
        case CommandType::MEDIA_NEXT:
        case CommandType::CAMERA_PREVIEW_START:
        case CommandType::CAMERA_PREVIEW_STOP: return true;
    }
    return false;
}

const std::string* parameter(const VehicleCommand& command, const std::string& key) {
    for (const auto& item : command.parameters) if (item.first == key) return &item.second;
    return nullptr;
}

protocol::Status exact_parameters(const VehicleCommand& command,
                                  const std::vector<std::string>& required,
                                  const std::vector<std::string>& optional = {}) {
    std::set<std::string> seen;
    for (const auto& item : command.parameters) {
        if (item.first.empty() || item.first.size() > 64 || item.second.size() > 256)
            return {protocol::StatusCode::INVALID_ARGUMENT, "parameter length"};
        if (!seen.insert(item.first).second)
            return {protocol::StatusCode::INVALID_ARGUMENT, "duplicate parameter"};
        if (std::find(required.begin(), required.end(), item.first) == required.end() &&
            std::find(optional.begin(), optional.end(), item.first) == optional.end())
            return {protocol::StatusCode::INVALID_ARGUMENT, "unexpected parameter"};
    }
    for (const auto& key : required) {
        if (seen.count(key) == 0) return {protocol::StatusCode::INVALID_ARGUMENT, "missing parameter"};
    }
    return protocol::Status::Ok();
}

void put16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value >> 8));
    out.push_back(static_cast<std::uint8_t>(value));
}

bool get16(const std::vector<std::uint8_t>& in, std::size_t& pos, std::uint16_t& value) {
    if (in.size() - pos < 2) return false;
    value = static_cast<std::uint16_t>((static_cast<std::uint16_t>(in[pos]) << 8) | in[pos + 1]);
    pos += 2;
    return true;
}

bool get_string(const std::vector<std::uint8_t>& in, std::size_t& pos, std::string& value) {
    std::uint16_t size = 0;
    if (!get16(in, pos, size) || in.size() - pos < size) return false;
    value.assign(reinterpret_cast<const char*>(in.data() + pos), size);
    pos += size;
    return true;
}

}  // namespace

protocol::Status validate_command_shape(const VehicleCommand& command,
                                        protocol::BootEpoch current_epoch,
                                        protocol::Deadline now_ms) {
    if (command.protocol_version != protocol::kProtocolVersion)
        return {protocol::StatusCode::UNSUPPORTED_VERSION, "vehicle command version"};
    if (command.message_type != protocol::MessageType::VEHICLE_COMMAND)
        return {protocol::StatusCode::MALFORMED, "vehicle command message type"};
    if (!known_source(command.source))
        return {protocol::StatusCode::INVALID_ARGUMENT, "command source"};
    if (!known_command(command.command_type))
        return {protocol::StatusCode::INVALID_ARGUMENT, "unsupported command"};
    if (command.request_id == 0)
        return {protocol::StatusCode::INVALID_ARGUMENT, "request id"};
    if (command.boot_epoch != current_epoch)
        return {protocol::StatusCode::STALE_EPOCH, "boot epoch"};
    if (command.deadline_ms == 0)
        return {protocol::StatusCode::INVALID_ARGUMENT, "finite deadline required"};
    if (now_ms > command.deadline_ms)
        return {protocol::StatusCode::EXPIRED, "command deadline"};
    if (command.source == CommandSource::VOICE && command.session_id == 0)
        return {protocol::StatusCode::INVALID_ARGUMENT, "voice session id"};
    if (command.source == CommandSource::REMOTE && command.command_type != CommandType::QUERY_STATE)
        return {protocol::StatusCode::INVALID_STATE, "remote source not authorized"};

    protocol::Status shape;
    switch (command.command_type) {
        case CommandType::CAMERA_SELECT: {
            shape = exact_parameters(command, {"camera"});
            const auto* camera = parameter(command, "camera");
            if (shape.ok() && camera != nullptr && *camera != "front" && *camera != "rear")
                return {protocol::StatusCode::INVALID_ARGUMENT, "camera"};
            break;
        }
        case CommandType::MEDIA_PLAY:
            shape = exact_parameters(command, {}, {"item"});
            if (shape.ok()) {
                const auto* item = parameter(command, "item");
                if (item != nullptr && item->empty())
                    return {protocol::StatusCode::INVALID_ARGUMENT, "media item"};
            }
            break;
        case CommandType::SIM_LED_SET:
        case CommandType::SIM_BUZZER_SET: {
            shape = exact_parameters(command, {"enabled"});
            const auto* enabled = parameter(command, "enabled");
            if (shape.ok() && enabled != nullptr && *enabled != "true" && *enabled != "false")
                return {protocol::StatusCode::INVALID_ARGUMENT, "enabled"};
            break;
        }
        default: shape = exact_parameters(command, {}); break;
    }
    return shape;
}

protocol::Status encode_command_message(const VehicleCommand& command, protocol::Message& message) {
    if (command.parameters.size() > std::numeric_limits<std::uint16_t>::max())
        return {protocol::StatusCode::INVALID_ARGUMENT, "parameter count"};
    std::vector<std::uint8_t> payload;
    put16(payload, static_cast<std::uint16_t>(command.source));
    put16(payload, static_cast<std::uint16_t>(command.command_type));
    put16(payload, static_cast<std::uint16_t>(command.parameters.size()));
    for (const auto& item : command.parameters) {
        if (item.first.size() > std::numeric_limits<std::uint16_t>::max() ||
            item.second.size() > std::numeric_limits<std::uint16_t>::max())
            return {protocol::StatusCode::INVALID_ARGUMENT, "parameter wire length"};
        put16(payload, static_cast<std::uint16_t>(item.first.size()));
        payload.insert(payload.end(), item.first.begin(), item.first.end());
        put16(payload, static_cast<std::uint16_t>(item.second.size()));
        payload.insert(payload.end(), item.second.begin(), item.second.end());
    }
    message.header = {command.protocol_version, command.message_type,
                      static_cast<std::uint32_t>(payload.size()), command.request_id,
                      command.session_id, command.boot_epoch, command.deadline_ms};
    message.payload = std::move(payload);
    return protocol::Status::Ok();
}

CommandDecodeResult decode_command_message(const protocol::Message& message) {
    VehicleCommand command;
    command.protocol_version = message.header.protocol_version;
    command.message_type = message.header.message_type;
    command.request_id = message.header.request_id;
    command.session_id = message.header.session_id;
    command.boot_epoch = message.header.boot_epoch;
    command.deadline_ms = message.header.deadline_ms;
    if (message.header.message_type != protocol::MessageType::VEHICLE_COMMAND)
        return {{protocol::StatusCode::MALFORMED, "vehicle message type"}, {}};
    std::size_t pos = 0;
    std::uint16_t source = 0;
    std::uint16_t type = 0;
    std::uint16_t count = 0;
    if (!get16(message.payload, pos, source) || !get16(message.payload, pos, type) ||
        !get16(message.payload, pos, count))
        return {{protocol::StatusCode::MALFORMED, "vehicle payload header"}, {}};
    command.source = static_cast<CommandSource>(source);
    command.command_type = static_cast<CommandType>(type);
    for (std::uint16_t i = 0; i < count; ++i) {
        std::string key;
        std::string value;
        if (!get_string(message.payload, pos, key) || !get_string(message.payload, pos, value))
            return {{protocol::StatusCode::MALFORMED, "vehicle parameter"}, {}};
        command.parameters.emplace_back(std::move(key), std::move(value));
    }
    if (pos != message.payload.size())
        return {{protocol::StatusCode::MALFORMED, "vehicle payload trailing bytes"}, {}};
    return {protocol::Status::Ok(), std::move(command)};
}

std::string command_fingerprint(const VehicleCommand& command) {
    auto parameters = command.parameters;
    std::sort(parameters.begin(), parameters.end());
    std::ostringstream out;
    out << command.protocol_version << ':' << static_cast<unsigned>(command.message_type) << ':'
        << command.session_id << ':' << command.boot_epoch << ':' << command.deadline_ms << ':'
        << static_cast<unsigned>(command.source) << ':' << static_cast<unsigned>(command.command_type);
    for (const auto& item : parameters) out << ':' << item.first.size() << ':' << item.first << ':' << item.second.size() << ':' << item.second;
    return out.str();
}

}  // namespace cockpit::vehicle
