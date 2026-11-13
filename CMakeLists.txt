cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

include(compilerconfig)
include(defaults)
include(helpers)

if(NOT OS_WINDOWS)
  message(FATAL_ERROR "${_name} currently supports Windows only (VST2/VST3 editors are hosted as Win32 windows).")
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/vst3sdk.cmake")

# Plugin hosting core, shared by the OBS module and the out-of-process scanner. No OBS or Qt dependency.
add_library(vstex-host STATIC)
target_sources(
  vstex-host
  PRIVATE
    src/host/plugin-host.cpp
    src/host/plugin-host.h
    src/host/vst2-abi.h
    src/host/vst2-instance.cpp
    src/host/vst3-instance.cpp
)
target_include_directories(vstex-host PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(vstex-host PUBLIC vst3sdk_hosting)
target_compile_definitions(vstex-host PUBLIC NOMINMAX)

# Scanning loads every plugin once; doing it in a child process means a crashing plugin can't take OBS down.
add_executable(vst-ex-scanner src/scanner/scanner-main.cpp)
target_link_libraries(vst-ex-scanner PRIVATE vstex-host)
install(TARGETS vst-ex-scanner RUNTIME DESTINATION "vst-enigmax-loader/bin/64bit")
add_custom_command(
  TARGET vst-ex-scanner
  POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/rundir/$<CONFIG>"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:vst-ex-scanner>" "${CMAKE_CURRENT_BINARY_DIR}/rundir/$<CONFIG>"
  VERBATIM
)

# DSP self-check: `cmake --build ... --target pitch-shifter-test` then run it (non-zero exit = failure).
add_executable(pitch-shifter-test EXCLUDE_FROM_ALL src/dsp/pitch-shifter-test.cpp)
target_include_directories(pitch-shifter-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
add_executable(beat-align-test EXCLUDE_FROM_ALL src/dsp/beat-align-test.cpp)
target_include_directories(beat-align-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
add_executable(peak-ring-test EXCLUDE_FROM_ALL src/dsp/peak-ring-test.cpp)
target_include_directories(peak-ring-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
add_executable(juce-format-test EXCLUDE_FROM_ALL src/loader/juce-format-test.cpp)
target_include_directories(juce-format-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)
find_package(Qt6 COMPONENTS Widgets Core REQUIRED)

# VST Enigmax Loader: VST2/VST3 hosting as its own OBS module, so VST Ex can be left out (or removed)
# without the VST filters, and the loader works on its own in OBS's standard Filters window.
# End-to-end check of the virtual track on a real libobs; see the file header for how to run it.
add_executable(master-virtual-test EXCLUDE_FROM_ALL src/master-virtual-test.cpp src/master-virtual.h)
target_include_directories(master-virtual-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(master-virtual-test PRIVATE OBS::libobs)

add_library(vst-enigmax-loader MODULE)
add_dependencies(vst-enigmax-loader vst-ex-scanner)
target_compile_definitions(vst-enigmax-loader PRIVATE VST_ENIGMAX_LOADER LOADER_VERSION="${_version}")
target_include_directories(vst-enigmax-loader PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(vst-enigmax-loader PRIVATE OBS::libobs OBS::obs-frontend-api Qt6::Core Qt6::Widgets vstex-host)
set_target_properties(vst-enigmax-loader PROPERTIES AUTOMOC ON PREFIX "")
target_sources(
  vst-enigmax-loader
  PRIVATE
    src/loader/loader-main.cpp
    src/loader/loader-dialog.cpp
    src/loader/loader-dialog.h
    src/loader/handover.cpp
    src/loader/handover.h
    src/loader/juce-format.h
    src/loader/loader-support.c
    src/plugin-support.h
    src/vst-filter.cpp
    src/vst-filter.h
    src/plugin-meta.cpp
    src/plugin-meta.h
    src/plugin-scanner.cpp
    src/plugin-scanner.h
    src/ui/editor-window.cpp
    src/ui/editor-window.h
    src/ui/plugin-picker.cpp
    src/ui/plugin-picker.h
    src/ui/ui-text.h
)
install(TARGETS vst-enigmax-loader LIBRARY DESTINATION "vst-enigmax-loader/bin/64bit")
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/data-loader/" DESTINATION "vst-enigmax-loader/data")

add_library(${CMAKE_PROJECT_NAME} MODULE)
target_link_libraries(
  ${CMAKE_PROJECT_NAME}
  PRIVATE OBS::libobs OBS::obs-frontend-api Qt6::Core Qt6::Widgets vstex-host winmm
)
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON)

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.cpp
    src/log-file.cpp
    src/log-file.h
    src/loader-api.h
    src/master-bus.cpp
    src/master-bus.h
    src/master-virtual.h
    src/vst-filter.h
    src/plugin-meta.cpp
    src/plugin-meta.h
    src/pitch-filter.cpp
    src/pitch-filter.h
    src/stereo-filter.cpp
    src/stereo-filter.h
    src/dsp/peak-ring.h
    src/dsp/pseudo-stereo.h
    src/dsp/pitch-shifter.h
    src/switch-sets.cpp
    src/switch-sets.h
    src/ui/backup.cpp
    src/ui/backup.h
    src/ui/chain-view.cpp
    src/ui/chain-view.h
    src/ui/ex-filters-dialog.cpp
    src/ui/ex-filters-dialog.h
    src/ui/filter-history.cpp
    src/ui/filter-history.h
    src/ui/menu-hook.cpp
    src/ui/fx-dock.cpp
    src/ui/fx-dock.h
    src/ui/menu-hook.h
    src/ui/cb-menu.h
    src/ui/window-memory.h
    src/ui/master-dock.cpp
    src/ui/master-dock.h
    src/ui/properties-panel.cpp
    src/ui/properties-panel.h
    src/ui/switch-dock.cpp
    src/ui/sync-dialog.cpp
    src/ui/sync-dialog.h
    src/dsp/beat-align.h
    src/ui/switch-dock.h
    src/ui/tools-dialog.cpp
    src/ui/tools-dialog.h
    src/ui/waveline-dock.cpp
    src/ui/waveline-dock.h
    src/ui/ui-text.h
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
