add_library(cockpit_vehicle_core STATIC
  src/client.cpp
  src/clock.cpp
  src/command.cpp
  src/core.cpp
  src/service_adapter.cpp
  src/service_registry.cpp
  src/state.cpp
  src/voice_command_sink.cpp
)
target_include_directories(cockpit_vehicle_core PUBLIC include)
target_link_libraries(cockpit_vehicle_core PUBLIC cockpit_voice cockpit_ipc Threads::Threads)
target_compile_features(cockpit_vehicle_core PUBLIC cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(cockpit_vehicle_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
