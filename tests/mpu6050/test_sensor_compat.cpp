#include "cockpit/protocol/message.hpp"
#include "cockpit/protocol/sensor_v1.h"
#include <cassert>
#include <vector>
int main() {
 sv_frame frame{};frame.type=SV_HELLO;frame.size=16;frame.request=0x0102030405060708;frame.session=2;frame.core_epoch=3;
 sv_put(frame.payload,1,2);sv_put(frame.payload+2,1,2);sv_put(frame.payload+4,99,8);sv_put(frame.payload+12,2,2);
 uint8_t bytes[256];size_t size;
 assert(sv_encode(&frame,bytes,sizeof(bytes),&size)==SV_OK);
 std::vector<uint8_t> wire(bytes,bytes+size);
 auto message=cockpit::protocol::decode(wire);assert(message.status.ok());
 assert(message.message.header.message_type==cockpit::protocol::MessageType::SENSOR_HELLO);
 assert(message.message.header.request_id==frame.request&&message.message.header.boot_epoch==3&&message.message.header.deadline_ms==0);
 std::vector<uint8_t> reencoded;assert(cockpit::protocol::encode(message.message,reencoded).ok()&&reencoded==wire);
 assert(!cockpit::protocol::is_known_type(static_cast<cockpit::protocol::MessageType>(25)));
 assert(!cockpit::protocol::is_known_type(static_cast<cockpit::protocol::MessageType>(0x105)));
 return 0;
}
