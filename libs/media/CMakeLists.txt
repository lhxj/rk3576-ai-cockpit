option(COCKPIT_ENABLE_V4L2_CAMERA "Build the Linux V4L2 MPLANE camera backend" OFF)

add_library(cockpit_media STATIC
  src/fake_camera_capture.cpp
  src/nv12_rgb.cpp
  src/preview_mailbox.cpp
)
target_include_directories(cockpit_media PUBLIC include)
target_link_libraries(cockpit_media PUBLIC Threads::Threads)
target_compile_features(cockpit_media PUBLIC cxx_std_17)

if(COCKPIT_ENABLE_V4L2_CAMERA)
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "COCKPIT_ENABLE_V4L2_CAMERA requires Linux")
  endif()
  include(CheckIncludeFileCXX)
  check_include_file_cxx("linux/videodev2.h" COCKPIT_HAVE_VIDEODEV2_H)
  if(NOT COCKPIT_HAVE_VIDEODEV2_H)
    message(FATAL_ERROR "COCKPIT_ENABLE_V4L2_CAMERA=ON but linux/videodev2.h is missing")
  endif()
  target_sources(cockpit_media PRIVATE src/v4l2_mplane_camera_capture.cpp)
  target_compile_definitions(cockpit_media PUBLIC COCKPIT_ENABLE_V4L2_CAMERA=1)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(cockpit_media PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
