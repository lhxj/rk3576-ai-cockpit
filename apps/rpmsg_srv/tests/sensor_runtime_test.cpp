#include "fake_sensor_transport.hpp"
#include <cassert>
#include <stdexcept>
struct ThrowTransport:cockpit::rpmsg::SensorTransport {
 unsigned mode;std::atomic<unsigned> closes{0};
 explicit ThrowTransport(unsigned m):mode(m){}
 bool open()override{if(mode==0)throw std::runtime_error("open");return true;}
 cockpit::rpmsg::LinkState state()override{if(mode==1)throw std::runtime_error("state");return {true,1};}
 bool send(const std::vector<uint8_t>&)override{if(mode==2)return false;return true;}
 bool receive(std::vector<uint8_t>&,unsigned)override{throw std::runtime_error("HUP");}
 void wake()override{}
 void close()override{closes++;}
};
int main(){
 using namespace cockpit::rpmsg;
 for(unsigned mode=0;mode<5;mode++){
  auto transport=std::make_unique<ThrowTransport>(mode);auto* ptr=transport.get();std::atomic<unsigned> reports{0};
  SensorRuntime runtime(std::move(transport),1,[&](const auto&){reports++;},[mode]()->uint64_t{if(mode==4)throw std::runtime_error("getrandom");return 3;});
  assert(runtime.start());std::this_thread::sleep_for(std::chrono::milliseconds(40));
  auto start=std::chrono::steady_clock::now();runtime.stop();assert(std::chrono::steady_clock::now()-start<std::chrono::seconds(1));assert(ptr->closes>0);
 }
 auto counters=std::make_shared<FakeSensorCounters>();std::atomic<unsigned> valid{0};
 SensorRuntime happy(std::make_unique<FakeSensorTransport>(counters),5,[&](const auto&s){if(s.has_value)valid++;});
 assert(happy.start());auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);while(!valid&&std::chrono::steady_clock::now()<end)std::this_thread::sleep_for(std::chrono::milliseconds(5));assert(valid);
 happy.stop();assert(counters->unsub==1&&counters->closed>0);
}
