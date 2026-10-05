#include "cockpit_ui/core_integration_runtime.h"
#include "cockpit_ui/main_window.h"
#include "fake_sensor_transport.hpp"
#include "pages/vehicle_page.h"
#include "cockpit_ui/vehicle_core_ui_backend.h"
#include <QApplication>
#include <QLabel>
#include <QCoreApplication>
#include <cassert>
#include <thread>
#include <limits>
int main(int argc,char**argv){
 QApplication app(argc,argv);
 auto counts=std::make_shared<FakeSensorCounters>();
 cockpit::ui::CoreIntegrationRuntimeOptions options;
 cockpit::ui::CoreIntegrationRuntime runtime(options,{},{},{},{},{},std::make_unique<FakeSensorTransport>(counts));
 assert(runtime.start());
 {
  cockpit::ui::MainWindow window(runtime.makeUiBackend());window.show();
  auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);bool shown=false;
  while(std::chrono::steady_clock::now()<end){QCoreApplication::processEvents();
   auto* value=window.findChild<QLabel*>("sensor_value_2");auto* detail=window.findChild<QLabel*>("sensor_detail");
   if(value&&detail&&value->text().contains("1.000 g / VALID")&&detail->text().contains("source=RUNTIME")){shown=true;break;}
   std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  assert(shown);auto canonical=runtime.core().get_snapshot().sensor_state;
  assert(canonical.has_value&&canonical.accel_raw[2]==16384&&canonical.accel_g[2]==1&&canonical.gyro_dps[0]==-1&&canonical.chip_temp_c==37.53);
  auto wrong=canonical;wrong.config_id=1;assert(!runtime.core().report_sensor_state(wrong).ok());
  wrong=canonical;wrong.accel_g[0]=std::numeric_limits<double>::quiet_NaN();assert(!runtime.core().report_sensor_state(wrong).ok());
  wrong=canonical;wrong.remote_epoch=0;assert(!runtime.core().report_sensor_state(wrong).ok());
  canonical.data=cockpit::vehicle::SensorDataCondition::STALE;canonical.age_ms=1000;
  // Direct canonical update validates stale projection independently of Fake transport timing.
  cockpit::ui::VehiclePage page;page.setState(cockpit::ui::mapVehicleState(runtime.core().get_snapshot()));
  auto mapped=cockpit::ui::mapVehicleState(runtime.core().get_snapshot());mapped.sensor_values=canonical;page.setState(mapped);
  for(int i=0;i<25;i++){QCoreApplication::processEvents();std::this_thread::sleep_for(std::chrono::milliseconds(5));}
  assert(page.findChild<QLabel*>("sensor_value_2")->text().contains("1.000 g / STALE"));
  assert(page.findChild<QLabel*>("sensor_detail")->text().contains("age=1000 ms"));
  runtime.stop();assert(counts->unsub==1&&counts->closed>0);assert(!runtime.core().running());
 }
 // Page no-data rendering through real mapping preserves explicit -- and source.
 cockpit::ui::CoreIntegrationRuntime fresh;assert(fresh.start());
 {cockpit::ui::MainWindow window(fresh.makeUiBackend());window.show();
  for(int i=0;i<25;i++){QCoreApplication::processEvents();std::this_thread::sleep_for(std::chrono::milliseconds(5));}
  auto* v=window.findChild<QLabel*>("sensor_value_0");assert(v&&v->text().contains("--"));
 }
 fresh.stop();
}
