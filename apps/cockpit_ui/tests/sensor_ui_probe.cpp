// SPDX-License-Identifier: MIT
// Finite real Sensor page observation. Human confirmation is collected separately.
#include "cockpit_ui/core_integration_runtime.h"
#include "cockpit_ui/main_window.h"
#include <QApplication>
#include <QTimer>
#include <iostream>
#include <chrono>
int main(int argc,char**argv){
 std::cout<<std::unitbuf;
 QApplication app(argc,argv);
 const auto args=app.arguments();
 if(args.size()!=2||args[1]!=QStringLiteral("--observe-real-sensor"))return 2;
 cockpit::ui::CoreIntegrationRuntimeOptions options;options.sensor_device_path="/dev/rk3576-sensor-v1";
 cockpit::ui::CoreIntegrationRuntime runtime(options);
 if(!runtime.start())return 3;
 cockpit::ui::MainWindow window(runtime.makeUiBackend());window.showFullScreen();window.showPage(cockpit::ui::PageId::Vehicle);
 auto began=std::chrono::steady_clock::now();QTimer timer;bool failure=false;unsigned observations=0;std::uint64_t previous_seq=0;
 QObject::connect(&timer,&QTimer::timeout,[&]{
  const auto s=runtime.core().get_snapshot().sensor_state;
  const auto elapsed=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-began).count();
  std::cout<<"SENSOR_UI elapsed_s="<<elapsed<<" session="<<s.session_id<<" epoch="<<s.remote_epoch<<" subscription="<<s.subscription_id<<" seq="<<s.sample_seq<<" pubseq="<<s.publish_seq<<" age_ms="<<s.age_ms<<" valid="<<(s.data==cockpit::vehicle::SensorDataCondition::VALID)<<" ax="<<s.accel_g[0]<<" ay="<<s.accel_g[1]<<" az="<<s.accel_g[2]<<" gx="<<s.gyro_dps[0]<<" gy="<<s.gyro_dps[1]<<" gz="<<s.gyro_dps[2]<<" chip_C="<<s.chip_temp_c<<" gaps="<<s.sequence_gaps<<" sample_errors="<<s.sample_errors<<" protocol_errors="<<s.protocol_errors<<" ui_merges="<<window.sensorMergeCount()<<std::endl;
  if(s.data==cockpit::vehicle::SensorDataCondition::VALID)observations++;
  if(elapsed>=5 && (s.sample_seq<=previous_seq || s.source!=cockpit::vehicle::StateSource::RUNTIME || !s.rtos_online || !s.rpmsg_online || !s.mpu_available)){failure=true;app.quit();}
  previous_seq=s.sample_seq;
  if(elapsed>=5 && (s.data!=cockpit::vehicle::SensorDataCondition::VALID||s.sample_errors||s.protocol_errors)){failure=true;app.quit();}
  if(elapsed>=90)app.quit();
 });
 timer.start(1000);app.exec();timer.stop();runtime.stop();
 const auto last=runtime.core().get_snapshot().sensor_state;
 std::cout<<"SENSOR_UI_EXIT observations="<<observations<<" unsubscribe_confirmed="<<last.unsubscribe_confirmed<<" ui_merges="<<window.sensorMergeCount()<<" human_confirmation=USER_CONFIRMATION_PENDING"<<std::endl;
 return !failure&&observations>=5&&last.unsubscribe_confirmed?0:4;
}
