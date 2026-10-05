add_library(cockpit_ui_core
  src/mock_ui_backend.cpp
  src/ui_state.cpp
)

target_include_directories(cockpit_ui_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(cockpit_ui_core PUBLIC cxx_std_17)
target_link_libraries(cockpit_ui_core PUBLIC cockpit_vehicle_core)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(cockpit_ui_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_library(cockpit_ui_vehicle_backend
  src/core_integration_runtime.cpp
  src/vehicle_core_ui_backend.cpp
  src/vision_ui_backend.cpp
)
target_include_directories(cockpit_ui_vehicle_backend
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(cockpit_ui_vehicle_backend
  PUBLIC cockpit_ui_core cockpit_vehicle_core cockpit_sensor_runtime cockpit_media_service cockpit_vision_runtime Threads::Threads
)
if(COCKPIT_ENABLE_RKNN)
  target_link_libraries(cockpit_ui_vehicle_backend PUBLIC cockpit_rknn_vision)
  target_compile_definitions(cockpit_ui_vehicle_backend PUBLIC COCKPIT_ENABLE_RKNN=1)
endif()
target_compile_features(cockpit_ui_vehicle_backend PUBLIC cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(cockpit_ui_vehicle_backend PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
if(BUILD_TESTING)
  add_executable(sensor_coexistence_gate_test tests/coexistence_consumer_gate_test.cpp)
  target_link_libraries(sensor_coexistence_gate_test PRIVATE cockpit_ui_vehicle_backend)
  target_compile_options(sensor_coexistence_gate_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  add_test(NAME sensor_coexistence_consumer_gate COMMAND sensor_coexistence_gate_test)
  add_executable(cockpit_ui_vehicle_backend_test tests/vehicle_core_backend_test.cpp)
  target_link_libraries(cockpit_ui_vehicle_backend_test PRIVATE cockpit_ui_vehicle_backend)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(cockpit_ui_vehicle_backend_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME cockpit_ui_vehicle_backend COMMAND cockpit_ui_vehicle_backend_test)
endif()
if(BUILD_TESTING)
  add_executable(cockpit_ui_foundation_test tests/foundation_test.cpp)
  target_link_libraries(cockpit_ui_foundation_test PRIVATE cockpit_ui_core)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(cockpit_ui_foundation_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME cockpit_ui_foundation COMMAND cockpit_ui_foundation_test)
endif()

# The current target image uses Qt 5.15.8, so prefer Qt5. Qt6 remains a source-
# compatibility path for development hosts; neither package is downloaded here.
find_package(Qt5 5.12 COMPONENTS Widgets QUIET)
if(Qt5Widgets_FOUND)
  set(COCKPIT_QT_PACKAGE Qt5)
else()
  find_package(Qt6 COMPONENTS Widgets QUIET)
  if(Qt6Widgets_FOUND)
    set(COCKPIT_QT_PACKAGE Qt6)
  endif()
endif()

if(COCKPIT_QT_PACKAGE)
  add_library(cockpit_ui_widgets STATIC
    src/main_window.cpp
    src/qt_preview_bridge.cpp
    include/cockpit_ui/qt_preview_bridge.h
    pages/ai_page.cpp
    pages/camera_page.cpp
    pages/home_page.cpp
    pages/media_page.cpp
    pages/monitor_page.cpp
    pages/settings_page.cpp
    pages/vehicle_page.cpp
    widgets/navigation_button.cpp
    widgets/status_badge.cpp
    include/cockpit_ui/main_window.h
    pages/ai_page.h
    pages/camera_page.h
    pages/home_page.h
    pages/media_page.h
    pages/monitor_page.h
    pages/settings_page.h
    pages/vehicle_page.h
    widgets/navigation_button.h
    widgets/status_badge.h
  )
  set_target_properties(cockpit_ui_widgets PROPERTIES AUTOMOC ON)
  target_include_directories(cockpit_ui_widgets PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(cockpit_ui_widgets PUBLIC cockpit_ui_vehicle_backend ${COCKPIT_QT_PACKAGE}::Widgets)
  target_compile_features(cockpit_ui_widgets PUBLIC cxx_std_17)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(cockpit_ui_widgets PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  add_executable(cockpit_ui src/main.cpp)
  target_link_libraries(cockpit_ui PRIVATE cockpit_ui_widgets)
  target_compile_features(cockpit_ui PRIVATE cxx_std_17)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(cockpit_ui PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  if(BUILD_TESTING)
    add_executable(cockpit_ui_qt_core_integration_test tests/qt_core_integration_test.cpp)
    target_link_libraries(cockpit_ui_qt_core_integration_test PRIVATE cockpit_ui_widgets)
    target_compile_features(cockpit_ui_qt_core_integration_test PRIVATE cxx_std_17)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
      target_compile_options(cockpit_ui_qt_core_integration_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME cockpit_ui_qt_core_integration COMMAND cockpit_ui_qt_core_integration_test)
    set_tests_properties(cockpit_ui_qt_core_integration PROPERTIES
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(cockpit_ui_qt_cam0_preview_test tests/qt_cam0_preview_test.cpp)
    target_link_libraries(cockpit_ui_qt_cam0_preview_test PRIVATE cockpit_ui_widgets)
    target_compile_features(cockpit_ui_qt_cam0_preview_test PRIVATE cxx_std_17)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
      target_compile_options(cockpit_ui_qt_cam0_preview_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME cockpit_ui_qt_cam0_preview COMMAND cockpit_ui_qt_cam0_preview_test)
    set_tests_properties(cockpit_ui_qt_cam0_preview PROPERTIES
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME cockpit_ui_offscreen_startup
      COMMAND cockpit_ui --windowed --quit-after-ms=100)
    set_tests_properties(cockpit_ui_offscreen_startup PROPERTIES
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME cockpit_ui_core_offscreen_startup
      COMMAND cockpit_ui --backend core --profile normal --windowed --quit-after-ms=150)
    set_tests_properties(cockpit_ui_core_offscreen_startup PROPERTIES
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  endif()
  message(STATUS "cockpit_ui Qt shell enabled with ${COCKPIT_QT_PACKAGE}")
else()
  message(STATUS "UI_HOST_BUILD_SKIPPED_QT_NOT_FOUND: cockpit_ui_core and tests remain enabled")
endif()

if(COCKPIT_QT_PACKAGE AND BUILD_TESTING)
 add_executable(qt_sensor_pipeline_test tests/qt_sensor_pipeline_test.cpp ${PROJECT_SOURCE_DIR}/rtos/rpmsg/sensor_service.c ${PROJECT_SOURCE_DIR}/rtos/sensor/mpu6050.c)
 target_include_directories(qt_sensor_pipeline_test PRIVATE ${PROJECT_SOURCE_DIR}/apps/rpmsg_srv/tests ${PROJECT_SOURCE_DIR}/rtos/rpmsg ${PROJECT_SOURCE_DIR}/rtos/sensor)
 target_link_libraries(qt_sensor_pipeline_test PRIVATE cockpit_ui_widgets)
 target_compile_options(qt_sensor_pipeline_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
 add_test(NAME qt_sensor_actual_pipeline COMMAND qt_sensor_pipeline_test)
 set_tests_properties(qt_sensor_actual_pipeline PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

add_executable(sensor_stream_probe tests/sensor_stream_probe.cpp)
target_link_libraries(sensor_stream_probe PRIVATE cockpit_ui_vehicle_backend)
target_compile_options(sensor_stream_probe PRIVATE -Wall -Wextra -Wpedantic -Werror)

if(COCKPIT_QT_PACKAGE)
  add_executable(sensor_ui_probe tests/sensor_ui_probe.cpp)
  target_link_libraries(sensor_ui_probe PRIVATE cockpit_ui_widgets)
  target_compile_options(sensor_ui_probe PRIVATE -Wall -Wextra -Werror)
endif()

if(COCKPIT_QT_PACKAGE AND COCKPIT_ENABLE_RKNN AND COCKPIT_ENABLE_SHERPA_ASR AND COCKPIT_ENABLE_ALSA_CAPTURE)
  add_executable(sensor_coexistence_probe tests/sensor_coexistence_probe.cpp)
  target_compile_features(sensor_coexistence_probe PRIVATE cxx_std_17)
  target_compile_options(sensor_coexistence_probe PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_link_libraries(sensor_coexistence_probe PRIVATE cockpit_ui_widgets cockpit_rknn_vision cockpit_voice_runtime cockpit_sherpa_c_api cockpit_sherpa_vad cockpit_alsa_capture cockpit_vehicle_core)
endif()
