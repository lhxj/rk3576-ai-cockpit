# Explicit test-only injection via -DCMAKE_PROJECT_INCLUDE=<this file>.
# Keeps the original root, build entries and all 35 native CTest registrations.
add_executable(system_coexistence_probe
  "${CMAKE_CURRENT_LIST_DIR}/system_coexistence_probe.cpp")
target_compile_features(system_coexistence_probe PRIVATE cxx_std_17)
target_compile_options(system_coexistence_probe PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(system_coexistence_probe PRIVATE
  cockpit_ui_widgets cockpit_rknn_vision cockpit_voice_runtime
  cockpit_sherpa_c_api cockpit_sherpa_vad cockpit_alsa_capture cockpit_vehicle_core)
