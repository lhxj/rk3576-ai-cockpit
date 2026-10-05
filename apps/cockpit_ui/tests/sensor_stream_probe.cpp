// Explicit board bringup tool: no sensor access until --device is provided.
#include "cockpit_ui/core_integration_runtime.h"
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <fstream>
int main(int argc,char**argv){
 std::string device;unsigned limit=100;unsigned window=120000;
 for(int i=1;i<argc;i++){std::string arg=argv[i];if(arg=="--device"&&i+1<argc)device=argv[++i];else if(arg=="--samples"&&i+1<argc)limit=std::stoul(argv[++i]);else if(arg=="--window-ms"&&i+1<argc)window=std::stoul(argv[++i]);else return 2;}
 if(device.empty()||device!="/dev/rk3576-sensor-v1"||limit<1||limit>100||window<1000||window>120000){std::cerr<<"Require fixed sensor device, samples1..100, window1000..120000ms\n";return 2;}
 cockpit::ui::CoreIntegrationRuntimeOptions options;options.sensor_device_path=device;
 cockpit::ui::CoreIntegrationRuntime runtime(options);
 std::mutex mutex;std::condition_variable wake;unsigned count=0;uint64_t prior=0;bool stopped=false;cockpit::vehicle::SensorState last;
 runtime.core().subscribe_state([&](const cockpit::vehicle::VehicleState& state){
  const auto&s=state.sensor_state;std::lock_guard<std::mutex>lock(mutex);last=s;
  if(!stopped&&s.data==cockpit::vehicle::SensorDataCondition::VALID&&s.sample_seq!=prior&&count<limit){
   prior=s.sample_seq;++count;
   std::cout<<"SENSOR_SAMPLE {\"remote_epoch\":"<<s.remote_epoch<<",\"subscription\":"<<s.subscription_id<<",\"sample_seq\":"<<s.sample_seq<<",\"publish_seq\":"<<s.publish_seq<<",\"m0_ms\":"<<s.m0_ms<<",\"rx_ms\":"<<s.rx_ms<<",\"accel\":["<<s.accel_raw[0]<<","<<s.accel_raw[1]<<","<<s.accel_raw[2]<<"],\"temp\":"<<s.chip_temp_raw<<",\"gyro\":["<<s.gyro_raw[0]<<","<<s.gyro_raw[1]<<","<<s.gyro_raw[2]<<"],\"config_id\":"<<s.config_id<<",\"gaps\":"<<s.sequence_gaps<<",\"protocol_errors\":"<<s.protocol_errors<<"}"<<std::endl;
   wake.notify_all();
  }
 });
 if(!runtime.start())return 3;
 {std::unique_lock<std::mutex>lock(mutex);wake.wait_for(lock,std::chrono::milliseconds(window),[&]{return count>=limit;});stopped=true;}
 runtime.stop();
 std::cout<<"SENSOR_EXIT samples="<<count<<" unsubscribe_confirmed="<<last.unsubscribe_confirmed<<" lease_fallback_ms=5000 gaps="<<last.sequence_gaps<<" protocol_errors="<<last.protocol_errors<<std::endl;
 return count==limit&&last.unsubscribe_confirmed&&last.protocol_errors==0?0:4;
}
