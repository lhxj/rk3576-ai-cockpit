add_library(cockpit_media_service STATIC
  src/media_service.cpp
  src/real_media_service_adapter.cpp
)
target_include_directories(cockpit_media_service PUBLIC include)
target_link_libraries(cockpit_media_service
  PUBLIC cockpit_media cockpit_vehicle_core Threads::Threads)
target_compile_features(cockpit_media_service PUBLIC cxx_std_17)
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

add_executable(cockpit_voice_media_test tools/voice_media_test.cpp)
target_link_libraries(cockpit_voice_media_test PRIVATE
  cockpit_media_service cockpit_intent_dispatcher)
target_compile_features(cockpit_voice_media_test PRIVATE cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(cockpit_voice_media_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
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
  add_test(NAME voice_intent_real_media COMMAND cockpit_voice_media_test --backend fake --suite)
endif()
