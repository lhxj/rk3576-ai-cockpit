#include "cockpit/protocol/message.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::cerr << __FILE__ << ':' << __LINE__ << " failed: " #x << '\n'; return 1; } } while (false)

int main() {
    using namespace cockpit::protocol;
    Message msg{{kProtocolVersion, MessageType::LLM_REQUEST, 3, 17, 100, 9, 5000}, {1, 2, 3}};
    std::vector<std::uint8_t> wire;
    CHECK(encode(msg, wire).ok());
    CHECK(wire.size() == kWireHeaderSize + 3);
    auto decoded = decode(wire);
    CHECK(decoded.status.ok());
    CHECK(decoded.message.header.request_id == 17);
    CHECK(decoded.message.header.session_id == 100);
    CHECK(decoded.message.payload == msg.payload);

    auto bad = wire;
    bad[5] = 2;
    CHECK(decode(bad).status.code == StatusCode::UNSUPPORTED_VERSION);
    bad = wire;
    bad[11] = 4;
    CHECK(decode(bad).status.code == StatusCode::MALFORMED);
    bad = wire;
    bad[7] = 255;
    CHECK(decode(bad).status.code == StatusCode::MALFORMED);
    bad = wire;
    bad.pop_back();
    CHECK(decode(bad).status.code == StatusCode::MALFORMED);
    bad.assign(MAX_CONTROL_MESSAGE_SIZE + 1, 0);
    CHECK(decode(bad).status.code == StatusCode::MALFORMED);
    Message oversized = msg;
    oversized.payload.assign(MAX_CONTROL_MESSAGE_SIZE, 0);
    oversized.header.payload_size = static_cast<std::uint32_t>(oversized.payload.size());
    CHECK(encode(oversized, wire).code == StatusCode::INVALID_ARGUMENT);
    CHECK(static_cast<int>(MessageType::ACK) != static_cast<int>(MessageType::RESULT));

    RequestFence fence(9, 100, 2);
    CHECK(fence.accept(msg, 4999).ok());
    CHECK(fence.accept(msg, 4999).code == StatusCode::DUPLICATE_REQUEST);
    auto cancel = msg;
    cancel.header.message_type = MessageType::CANCEL;
    CHECK(fence.accept(cancel, 4999).ok());  // cancellation refers to the original request
    CHECK(fence.accept(msg, 5001).code == StatusCode::EXPIRED);
    auto old_session = msg;
    old_session.header.session_id = 99;
    CHECK(fence.accept(old_session, 4999).code == StatusCode::STALE_SESSION);
    auto old_epoch = msg;
    old_epoch.header.boot_epoch = 8;
    CHECK(fence.accept(old_epoch, 4999).code == StatusCode::STALE_EPOCH);
    auto chunk = msg;
    chunk.header.message_type = MessageType::LLM_CHUNK;
    CHECK(fence.accept(chunk, 4999).ok());
    CHECK(fence.accept(chunk, 4999).ok());
    fence.set_session(101);
    CHECK(fence.recent_size() == 0);
    return 0;
}
