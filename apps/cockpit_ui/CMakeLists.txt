add_library(cockpit_ui_core
  src/mock_ui_backend.cpp
  src/ui_state.cpp
)

target_include_directories(cockpit_ui_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(cockpit_ui_core PUBLIC cxx_std_17)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(cockpit_ui_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
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
  add_executable(cockpit_ui
    src/main.cpp
    src/main_window.cpp
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
  set_target_properties(cockpit_ui PROPERTIES AUTOMOC ON)
  target_include_directories(cockpit_ui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(cockpit_ui PRIVATE cockpit_ui_core ${COCKPIT_QT_PACKAGE}::Widgets)
  target_compile_features(cockpit_ui PRIVATE cxx_std_17)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(cockpit_ui PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  if(BUILD_TESTING)
    add_test(NAME cockpit_ui_offscreen_startup
      COMMAND cockpit_ui --windowed --quit-after-ms=100)
    set_tests_properties(cockpit_ui_offscreen_startup PROPERTIES
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  endif()
  message(STATUS "cockpit_ui Qt shell enabled with ${COCKPIT_QT_PACKAGE}")
else()
  message(STATUS "UI_HOST_BUILD_SKIPPED_QT_NOT_FOUND: cockpit_ui_core and tests remain enabled")
endif()
