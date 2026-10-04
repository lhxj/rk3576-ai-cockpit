add_library(cockpit_media_service STATIC
  src/fake_h264_encoder.cpp
  src/fake_rtsp_server.cpp
  src/file_recording_sink.cpp
  src/h264_annex_b.cpp
  src/media_service.cpp
  src/real_media_service_adapter.cpp
  src/rtp_h264.cpp
  src/rtsp_server.cpp
)
target_include_directories(cockpit_media_service PUBLIC include)
target_link_libraries(cockpit_media_service
  PUBLIC cockpit_media cockpit_vehicle_core Threads::Threads)
target_compile_features(cockpit_media_service PUBLIC cxx_std_17)
if(COCKPIT_ENABLE_MPP_RECORDING)
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "COCKPIT_ENABLE_MPP_RECORDING requires Linux")
  endif()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(ROCKCHIP_MPP REQUIRED IMPORTED_TARGET rockchip_mpp)
  target_sources(cockpit_media_service PRIVATE src/mpp_h264_encoder.cpp)
  target_link_libraries(cockpit_media_service PUBLIC PkgConfig::ROCKCHIP_MPP)
  target_compile_definitions(cockpit_media_service PUBLIC COCKPIT_ENABLE_MPP_RECORDING=1)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(cockpit_media_service PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(COCKPIT_ENABLE_V4L2_CAMERA)
  add_executable(media_cam0_probe tools/media_cam0_probe.cpp)
  target_link_libraries(media_cam0_probe PRIVATE cockpit_media_service)
  target_compile_features(media_cam0_probe PRIVATE cxx_std_17)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(media_cam0_probe PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endif()

if(COCKPIT_ENABLE_V4L2_CAMERA AND COCKPIT_ENABLE_MPP_RECORDING)
  add_executable(media_recording_probe tools/media_recording_probe.cpp)
  target_link_libraries(media_recording_probe PRIVATE cockpit_media_service)
  target_compile_features(media_recording_probe PRIVATE cxx_std_17)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(media_recording_probe PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()

  add_executable(media_rtsp_probe tools/media_rtsp_probe.cpp)
  target_link_libraries(media_rtsp_probe PRIVATE cockpit_media_service)
  target_compile_features(media_rtsp_probe PRIVATE cxx_std_17)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(media_rtsp_probe PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endif()

if(COCKPIT_ENABLE_V4L2_CAMERA AND COCKPIT_ENABLE_MPP_RECORDING AND COCKPIT_ENABLE_RKNN)
  add_executable(vision_rknn_probe tools/vision_rknn_probe.cpp)
  target_link_libraries(vision_rknn_probe PRIVATE
    cockpit_media_service cockpit_rknn_vision cockpit_vision_runtime)
  target_compile_features(vision_rknn_probe PRIVATE cxx_std_17)
  set_target_properties(vision_rknn_probe PROPERTIES SKIP_BUILD_RPATH TRUE)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(vision_rknn_probe PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endif()

add_executable(cockpit_voice_media_test tools/voice_media_test.cpp)
target_link_libraries(cockpit_voice_media_test PRIVATE
  cockpit_media_service cockpit_intent_dispatcher)
target_compile_features(cockpit_voice_media_test PRIVATE cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(cockpit_voice_media_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(COCKPIT_ENABLE_V4L2_CAMERA AND COCKPIT_ENABLE_MPP_RECORDING AND
   COCKPIT_ENABLE_SHERPA_ASR AND COCKPIT_ENABLE_ALSA_CAPTURE)
  add_executable(cockpit_voice_runtime_board_test tools/voice_runtime_board_test.cpp)
  target_link_libraries(cockpit_voice_runtime_board_test PRIVATE
    cockpit_media_service cockpit_voice_runtime cockpit_sherpa_vad
    cockpit_sherpa_c_api cockpit_alsa_capture)
  target_compile_features(cockpit_voice_runtime_board_test PRIVATE cxx_std_17)
  set_target_properties(cockpit_voice_runtime_board_test PROPERTIES SKIP_BUILD_RPATH TRUE)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(cockpit_voice_runtime_board_test PRIVATE
      -Wall -Wextra -Wpedantic -Werror)
  endif()
endif()

if(BUILD_TESTING)
  add_executable(media_service_test tests/media_service_test.cpp)
  target_link_libraries(media_service_test PRIVATE cockpit_media_service)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(media_service_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME media_service COMMAND media_service_test)

  add_executable(media_core_integration_test tests/media_core_integration_test.cpp)
  target_link_libraries(media_core_integration_test PRIVATE cockpit_media_service)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(media_core_integration_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME media_core_integration COMMAND media_core_integration_test)

  add_executable(media_recording_test tests/media_recording_test.cpp)
  target_link_libraries(media_recording_test PRIVATE cockpit_media_service)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(media_recording_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME media_recording COMMAND media_recording_test)

  add_executable(media_rtsp_test tests/media_rtsp_test.cpp)
  target_link_libraries(media_rtsp_test PRIVATE cockpit_media_service)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(media_rtsp_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME media_rtsp COMMAND media_rtsp_test)
  add_test(NAME voice_intent_real_media COMMAND cockpit_voice_media_test --backend fake --suite)
endif()
