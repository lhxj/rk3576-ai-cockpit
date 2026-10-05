#include "cockpit/rpmsg/sensor_client.hpp"
extern "C" {
#include "sensor_service.h"
}
#include <cassert>
#include <deque>
#include <iostream>
using namespace cockpit;
struct Fake {
 sensor_service service{};std::deque<std::vector<std::uint8_t>> received;unsigned initialized{0};
 static int send(void*arg,uint32_t,const uint8_t*data,size_t len){auto*f=static_cast<Fake*>(arg);f->received.emplace_back(data,data+len);return 0;}
 static int init(void*arg,uint64_t){static_cast<Fake*>(arg)->initialized++;return 0;}
 Fake(){sensor_service_init(&service,{this,send,init},0);}
 void exchange(rpmsg::SensorClient& client,uint64_t now){
  auto wire=client.take_outgoing();if(!wire.empty())assert(sensor_service_request(&service,7,wire.data(),wire.size(),now)==0);
  while(!received.empty()){auto w=std::move(received.front());received.pop_front();client.receive(w,1,now);}
 }
 void sample(uint64_t seq,uint64_t now){mpu_sample_state sample{};sample.valid=1;sample.sample_seq=seq;sample.last_valid_ms=now;sample.last.accel[0]=-16384;sample.last.gyro[1]=131;sample.last.temperature=340;sensor_service_offer(&service,&sample);sensor_service_poll(&service,now);}
};
int main(){
 vehicle::SensorState canonical;
 rpmsg::SensorClient client(91,17,123,[&](const auto&s){canonical=s;});Fake f;
 client.attach({true,1},0);f.exchange(client,0);f.exchange(client,0);
 // Subscription ACK alone cannot make SAMPLE acceptable.
 auto sub=client.take_outgoing();sv_frame request{};assert(!sv_decode(sub.data(),sub.size(),&request));
 sv_frame ack{};ack.type=SV_ACK;ack.request=request.request;ack.session=17;ack.core_epoch=91;ack.size=24;sv_put(ack.payload,1,2);sv_put(ack.payload+2,1,2);sv_put(ack.payload+4,123,8);sv_put(ack.payload+12,sv_get(request.payload+12,8),8);sv_put(ack.payload+22,SV_SUBSCRIBE,2);
 std::vector<uint8_t> wire(256);size_t len;assert(!sv_encode(&ack,wire.data(),wire.size(),&len));wire.resize(len);client.receive(wire,1,0);
 assert(sensor_service_request(&f.service,7,sub.data(),sub.size(),0)==0);
 f.sample(1,50);auto first=f.received.back();client.receive(first,1,50);assert(!canonical.has_value);
 f.exchange(client,50);assert(canonical.has_value&&canonical.accel_g[0]==-1&&canonical.gyro_dps[1]==1);
 assert(canonical.chip_temp_c==37.53&&canonical.remote_epoch==123&&canonical.m0_ms==50&&canonical.rx_ms==50);
 client.receive(first,1,60);assert(client.snapshot().duplicate==1&&client.snapshot().rx_ms==50);
 auto bad=first;bad[44+64]=4;client.receive(bad,1,70);assert(client.snapshot().protocol_errors==1);
 f.sample(2,100);f.exchange(client,100);
 f.service.publish_seq+=2;f.sample(3,150);f.exchange(client,150);assert(canonical.sequence_gaps==2);
 client.poll(650);assert(canonical.data==vehicle::SensorDataCondition::STALE&&canonical.age_ms==500);
 f.sample(4,700);f.exchange(client,700);assert(canonical.data==vehicle::SensorDataCondition::VALID&&canonical.session_id==17);
 client.receive(first,2,800);assert(client.snapshot().old_packets>0);
 client.stop(900);client.receive(first,1,901);assert(canonical.data==vehicle::SensorDataCondition::OFFLINE);
 f.exchange(client,900);assert(client.unsubscribe_confirmed());client.poll(1200);assert(canonical.age_ms==500);
 // New endpoint fences old wire; query new zero epoch then binds original bootstrap nonce, not queried prior epoch.
 rpmsg::SensorClient reconnect(91,18,456,[&](const auto&s){canonical=s;});Fake fresh;
 reconnect.attach({true,2},0);auto q=reconnect.take_outgoing();assert(!sv_decode(q.data(),q.size(),&request));
 sv_frame status{};status.type=SV_STATUS;status.request=request.request;status.session=18;status.core_epoch=91;status.size=72;sv_put(status.payload,1,2);sv_put(status.payload+2,1,2);status.payload[54]=3;status.payload[55]=49;status.payload[56]=1;sv_put(status.payload+58,20,2);sv_put(status.payload+60,SV_CONFIG_ID,4);
 wire.resize(256);assert(!sv_encode(&status,wire.data(),wire.size(),&len));wire.resize(len);reconnect.receive(wire,1,0);assert(reconnect.take_outgoing().empty());reconnect.receive(wire,2,0);auto bind=reconnect.take_outgoing();assert(!sv_decode(bind.data(),bind.size(),&request)&&sv_get(request.payload+4,8)==456);
 reconnect.disconnect();reconnect.poll(900);assert(!canonical.mpu_available&&!canonical.rpmsg_online);
 // Matching periodic QUERY is the sole control proof for an epoch transition.
 rpmsg::SensorClient epoch_client(91,19,789,[&](const auto&s){canonical=s;});Fake before;
 epoch_client.attach({true,1},0);before.exchange(epoch_client,0);before.exchange(epoch_client,0);before.exchange(epoch_client,0);
 before.sample(1,50);before.exchange(epoch_client,50);assert(canonical.has_value);
 epoch_client.poll(2000);before.exchange(epoch_client,2000);epoch_client.poll(2600);
 auto query=epoch_client.take_outgoing();assert(!sv_decode(query.data(),query.size(),&request)&&request.type==SV_HELLO);
 Fake reboot;assert(!sensor_service_request(&reboot.service,7,query.data(),query.size(),2600));
 auto current=reboot.received.front();reboot.received.pop_front();epoch_client.receive(current,1,2600);
 assert(!canonical.mpu_available&&canonical.subscription_id==0&&canonical.data==vehicle::SensorDataCondition::OFFLINE);
 before.sample(2,2700);before.exchange(epoch_client,2700);assert(!canonical.has_value);
 // Initial packet-length and generation failures never acquire runtime data.
 for(size_t n=0;n<44;n++){std::vector<uint8_t> truncated(n);epoch_client.receive(truncated,1,2700);}
 assert(epoch_client.snapshot().old_packets>=44);
 rpmsg::SensorClient late(91,20,900,[&](const auto&s){canonical=s;});Fake delayed;
 late.attach({true,1},0);auto late_query=late.take_outgoing();assert(!sensor_service_request(&delayed.service,7,late_query.data(),late_query.size(),0));
 late.receive(delayed.received.front(),1,1000);assert(canonical.data==vehicle::SensorDataCondition::OFFLINE&&late.take_outgoing().empty());
 std::cout<<"SENSOR_CLIENT_ACTUAL_RTOS_SERVICE_PASS\n";
}
