#include "cockpit/rpmsg/sensor_client.hpp"
#include <algorithm>
#include <utility>
namespace cockpit::rpmsg {
SensorClient::SensorClient(std::uint64_t core,std::uint64_t session,std::uint64_t nonce,Callback cb)
 :core_(core),session_(session),nonce_(nonce),callback_(std::move(cb)) {}
void SensorClient::publish(){if(callback_){try{callback_(state_);}catch(...){}}}
void SensorClient::fail(){phase_=Phase::OFFLINE;pending_=0;outgoing_.clear();state_.subscription_id=0;state_.subscription_active=false;state_.mpu_available=false;state_.rtos_online=state_.rpmsg_online=false;state_.data=vehicle::SensorDataCondition::OFFLINE;publish();}
void SensorClient::command(std::uint16_t type,std::uint64_t now){
 sv_frame f{};f.type=type;f.request=++request_;f.session=session_;f.core_epoch=core_;f.size=sv_payload_size(type);
 sv_put(f.payload,1,2);sv_put(f.payload+2,1,2);
 if(type==SV_HELLO){sv_put(f.payload+4,phase_==Phase::BIND?(state_.remote_epoch?state_.remote_epoch:nonce_):0,8);sv_put(f.payload+12,phase_==Phase::BIND?2:1,2);}
 else {sv_put(f.payload+4,state_.remote_epoch,8);sv_put(f.payload+12,state_.subscription_id,8);}
 if(type==SV_SUBSCRIBE){sv_put(f.payload+20,20,2);sv_put(f.payload+22,5000,2);}
 outgoing_.resize(SV_MAX_WIRE);size_t len=0;
 if(sv_encode(&f,outgoing_.data(),outgoing_.size(),&len)){fail();return;}
 outgoing_.resize(len);pending_=f.request;pending_type_=type;deadline_=now+1000;
}
void SensorClient::attach(LinkState link,std::uint64_t now){
 fail();if(!link.ready||!link.generation||!core_||!session_||!nonce_)return;
 generation_=link.generation;state_.generation=generation_;state_.session_id=session_;state_.source=vehicle::StateSource::RUNTIME;
 state_.remote_epoch=0;state_.sample_seq=state_.publish_seq=0;state_.has_value=false;state_.mpu_available=false;
 state_.data=vehicle::SensorDataCondition::NO_DATA;phase_=Phase::QUERY;link_rx_=now;command(SV_HELLO,now);
}
void SensorClient::receive(const std::vector<std::uint8_t>& wire,std::uint64_t generation,std::uint64_t now){
 if(generation!=generation_||phase_==Phase::OFFLINE){state_.old_packets++;return;}
 sv_frame f{};if(sv_decode(wire.data(),wire.size(),&f)){state_.protocol_errors++;return;}
 if(f.session!=session_||f.core_epoch!=core_){state_.old_packets++;return;}
 if(pending_&&f.request==pending_&&now>=deadline_){fail();return;}
 const auto epoch=sv_get(f.payload+4,8);
 if(phase_==Phase::QUERY||phase_==Phase::BIND){
  if(f.type!=SV_STATUS||f.request!=pending_||pending_type_!=SV_HELLO){state_.old_packets++;return;}
  if(phase_==Phase::BIND&&epoch!=(state_.remote_epoch?state_.remote_epoch:nonce_)){fail();return;}
  pending_=0;state_.remote_epoch=epoch;query_at_=now+2500;
  if(!epoch){phase_=Phase::BIND;command(SV_HELLO,now);return;}
  // Query existing epoch first; READY claim retains it and establishes this control owner.
  if(phase_==Phase::QUERY){phase_=Phase::BIND;command(SV_HELLO,now);return;}
  state_.rtos_online=state_.rpmsg_online=true;state_.mpu_available=f.payload[48]==1;
  state_.error=sv_get(f.payload+50,2);link_rx_=now;
  if(!state_.mpu_available){state_.data=vehicle::SensorDataCondition::ERROR;phase_=Phase::ACTIVE;publish();return;}
  state_.subscription_id=++request_;phase_=Phase::SUBSCRIBE;command(SV_SUBSCRIBE,now);publish();return;
 }
 if(phase_==Phase::STOPPED && !(f.type==SV_RESULT&&pending_type_==SV_UNSUBSCRIBE&&f.request==pending_)) {state_.old_packets++;return;}
 if(phase_==Phase::ACTIVE&&f.type==SV_STATUS&&pending_type_==SV_HELLO&&f.request==pending_) {
  pending_=0;query_at_=now+2500;
  if(epoch!=state_.remote_epoch){
   state_.subscription_id=0;state_.data=vehicle::SensorDataCondition::OFFLINE;state_.mpu_available=false;state_.rtos_online=state_.rpmsg_online=false;publish();
   state_.remote_epoch=epoch;state_.has_value=false;state_.sample_seq=state_.publish_seq=0;
   if(!epoch){fail();return;} // runtime recreates session and fresh random nonce before any new bind
   phase_=Phase::BIND;command(SV_HELLO,now);return;
  }
  link_rx_=now;return;
 }
 if(epoch!=state_.remote_epoch){state_.old_packets++;return;}
 if(f.type==SV_RESULT||f.type==SV_ERROR||f.type==SV_ACK){
  if(f.request!=pending_||sv_get(f.payload+22,2)!=pending_type_){state_.old_packets++;return;}
  if(f.type==SV_ACK)return;
  if(sv_get(f.payload+20,2)){fail();return;}
  if(pending_type_==SV_SUBSCRIBE&&sv_get(f.payload+12,8)!=state_.subscription_id){state_.old_packets++;return;}
  pending_=0;link_rx_=now;
  if(pending_type_==SV_SUBSCRIBE){phase_=Phase::ACTIVE;state_.subscription_active=true;renew_=now+2000;}
  if(pending_type_==SV_UNSUBSCRIBE){unsubscribe_confirmed_=true;state_.unsubscribe_confirmed=true;publish();}
  return;
 }
 if(phase_!=Phase::ACTIVE){state_.old_packets++;return;}
 if(f.type==SV_STATUS){
  if(sv_get(f.payload+12,8)!=state_.subscription_id){state_.old_packets++;return;}
  state_.sample_errors=sv_get(f.payload+36,4);state_.latest_overwrites=sv_get(f.payload+40,4);state_.send_failures=sv_get(f.payload+44,4);
  state_.error=sv_get(f.payload+50,2);state_.control_drops=sv_get(f.payload+68,4);state_.mpu_available=f.payload[48]==1;
  if(!f.payload[49]||!state_.mpu_available||state_.error)state_.data=vehicle::SensorDataCondition::ERROR;
  link_rx_=now;publish();return;
 }
 if(f.type!=SV_SAMPLE||sv_get(f.payload+12,8)!=state_.subscription_id){state_.old_packets++;return;}
 const auto seq=sv_get(f.payload+20,8),pub=sv_get(f.payload+28,8);
 if(seq<state_.sample_seq||pub<state_.publish_seq){state_.out_of_order++;return;}
 if(seq==state_.sample_seq||pub==state_.publish_seq){state_.duplicate++;return;}
 if(state_.publish_seq&&pub>state_.publish_seq+1)state_.sequence_gaps+=pub-state_.publish_seq-1;
 for(unsigned i=0;i<3;i++){state_.accel_raw[i]=sv_get_signed16(f.payload+48+i*2);state_.gyro_raw[i]=sv_get_signed16(f.payload+56+i*2);state_.accel_g[i]=state_.accel_raw[i]/16384.0;state_.gyro_dps[i]=state_.gyro_raw[i]/131.0;}
 state_.chip_temp_raw=sv_get_signed16(f.payload+54);state_.chip_temp_c=state_.chip_temp_raw/340.0+36.53;
 state_.m0_ms=sv_get(f.payload+36,8);state_.rx_ms=now;state_.age_ms=0;state_.sample_seq=seq;state_.publish_seq=pub;state_.config_id=sv_get(f.payload+68,4);
 state_.accel_fs=f.payload[62];state_.gyro_fs=f.payload[63];state_.dlpf=f.payload[64];state_.divider=f.payload[65];state_.power=f.payload[66];state_.m0_time_unit=f.payload[44];
 state_.data=vehicle::SensorDataCondition::VALID;state_.has_value=true;state_.error=0;state_.mpu_available=true;link_rx_=now;publish();
}
void SensorClient::link_counters(LinkState link){state_.linux_sample_overwrites=link.overwrites;state_.linux_control_drops=link.drops;state_.linux_malformed=link.malformed;state_.linux_send_failures=link.send_failures;}
void SensorClient::poll(std::uint64_t now){
 if(state_.has_value)state_.age_ms=now>=state_.rx_ms?now-state_.rx_ms:0;
 if(phase_==Phase::OFFLINE||phase_==Phase::STOPPED){publish();return;}
 if(pending_&&now>=deadline_){fail();return;}
 if(state_.has_value){state_.age_ms=now>=state_.rx_ms?now-state_.rx_ms:0;if(state_.age_ms>=500&&state_.data==vehicle::SensorDataCondition::VALID)state_.data=vehicle::SensorDataCondition::STALE;}
 if(now>=link_rx_+6000){fail();return;}
 if(phase_==Phase::ACTIVE&&!pending_){
  if(state_.subscription_id&&now>=renew_)command(SV_SUBSCRIBE,now);
  else if(now>=query_at_)command(SV_HELLO,now);
 }
 publish();
}
void SensorClient::disconnect(){fail();}
void SensorClient::stop(std::uint64_t now){if(state_.subscription_id&&phase_!=Phase::OFFLINE)command(SV_UNSUBSCRIBE,now);phase_=Phase::STOPPED;state_.subscription_active=false;state_.data=vehicle::SensorDataCondition::OFFLINE;state_.rtos_online=state_.rpmsg_online=false;publish();}
std::vector<std::uint8_t> SensorClient::take_outgoing(){auto out=std::move(outgoing_);outgoing_.clear();return out;}
}
