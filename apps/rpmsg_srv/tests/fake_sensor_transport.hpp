#pragma once
#include "cockpit/rpmsg/sensor_client.hpp"
extern "C" {
#include "sensor_service.h"
}
#include <deque>
#include <condition_variable>
#include <mutex>
#include <cstring>
#include <chrono>
struct FakeSensorCounters {std::atomic<unsigned> unsub{0},closed{0},writes{0};};
class FakeSensorTransport final:public cockpit::rpmsg::SensorTransport {
 mpu_device device_{};mpu_sample_state sample_state_{};uint8_t registers_[256]{};
 sensor_service service_{};std::deque<std::vector<uint8_t>> queue_;std::mutex mutex_;std::condition_variable wake_;
 std::shared_ptr<FakeSensorCounters> counts_;bool opened_{false},woken_{false};
 static uint64_t now(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
 static int send_fn(void*arg,uint32_t,const uint8_t*data,size_t len){auto*self=static_cast<FakeSensorTransport*>(arg);self->queue_.emplace_back(data,data+len);return 0;}
 static int read_i2c(void*arg,uint8_t,uint8_t reg,uint8_t*out,size_t size){
  auto*self=static_cast<FakeSensorTransport*>(arg);
  if(reg==0x3b&&size==14){const int16_t raw[7]={0,0,16384,340,-131,0,0};for(unsigned i=0;i<7;i++){out[i*2]=static_cast<uint16_t>(raw[i])>>8;out[i*2+1]=raw[i];}return 14;}
  memcpy(out,self->registers_+reg,size);return static_cast<int>(size);
 }
 static int write_i2c(void*arg,uint8_t,uint8_t reg,const uint8_t*data,size_t size){auto*self=static_cast<FakeSensorTransport*>(arg);memcpy(self->registers_+reg,data,size);if(reg==0x6b&&*data==0x80)self->registers_[reg]=0;return static_cast<int>(size);}
 static void wait_i2c(void*,uint32_t){}
 static uint64_t time_i2c(void*){return now();}
 static int initialize(void*arg,uint64_t){auto*self=static_cast<FakeSensorTransport*>(arg);return mpu_init(&self->device_,{self,read_i2c,write_i2c,wait_i2c,time_i2c},0x68);}
public:
 explicit FakeSensorTransport(std::shared_ptr<FakeSensorCounters> counts):counts_(std::move(counts)){registers_[0x75]=0x68;sensor_service_init(&service_,{this,send_fn,initialize},now());}
 bool open()override{std::lock_guard<std::mutex>lock(mutex_);opened_=true;return true;}
 cockpit::rpmsg::LinkState state()override{std::lock_guard<std::mutex>lock(mutex_);return {opened_,1};}
 bool send(const std::vector<uint8_t>&wire)override{std::lock_guard<std::mutex>lock(mutex_);sv_frame f{};if(sv_decode(wire.data(),wire.size(),&f))return false;counts_->writes++;if(f.type==SV_UNSUBSCRIBE)counts_->unsub++;return sensor_service_request(&service_,7,wire.data(),wire.size(),now())==0;}
 bool receive(std::vector<uint8_t>&wire,unsigned wait)override{
  std::unique_lock<std::mutex>lock(mutex_);wake_.wait_for(lock,std::chrono::milliseconds(wait),[&]{return woken_;});woken_=false;
  if(device_.ready){mpu_sample(&device_,&sample_state_);sensor_service_offer(&service_,&sample_state_);}
 sensor_service_poll(&service_,now());
  if(queue_.empty())return false;
  wire=std::move(queue_.front());queue_.pop_front();return true;
 }
 void wake()override{std::lock_guard<std::mutex>lock(mutex_);woken_=true;wake_.notify_all();}
 void close()override{std::lock_guard<std::mutex>lock(mutex_);opened_=false;counts_->closed++;}
};
