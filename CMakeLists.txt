# ORIGAMI: the stereo triple wave folder effect from Martial Systems. One DSP core (src/origami/dsp, framework-free,
# C++17) shared by the standalone plugin (VST3 + Standalone) and the Jidai Rack's ORIGAMI device.
#
# Standalone:  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build && ctest --test-dir build
# In a host:   FetchContent_Declare(origami GIT_REPOSITORY https://github.com/martialsystems/origami.git GIT_TAG <sha>)
#              FetchContent_MakeAvailable(origami), then link origami::dsp and compile ${ORIGAMI_UI_SOURCES}.
cmake_minimum_required(VERSION 3.22)
project(Origami VERSION 0.1.0 LANGUAGES C CXX)

if(PROJECT_IS_TOP_LEVEL)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

# ---- JUCE 8.0.4, fetched the same way as the Jidai Collection rack. A host that already made JUCE available
# (juce_add_plugin defined) is reused. Pass -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE for a local checkout. ----
if(NOT COMMAND juce_add_plugin)
  include(FetchContent)
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

# ---- jidai-common 1.1.1 (Jidai Cable Standard v1.1 + shared DSP), vendored verbatim in third_party/jidai-common.
# If the host already defines jidai::common (the Jidai Rack does), that target is used instead, so the host binary
# has a single jidai-common include path and one copy of each header. ----
if(TARGET jidai::common)
  message(STATUS "ORIGAMI: using the host's jidai::common (vendored third_party/jidai-common not added)")
else()
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/jidai-common ${CMAKE_CURRENT_BINARY_DIR}/jidai-common)
endif()

enable_testing()

# ---- DSP core: header-only, C++17, no JUCE. Include as "origami/dsp/OrigamiCore.h". ----
add_library(origami_dsp INTERFACE)
add_library(origami::dsp ALIAS origami_dsp)
target_include_directories(origami_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(origami_dsp INTERFACE jidai::common)
target_compile_features(origami_dsp INTERFACE cxx_std_17)

add_executable(OrigamiTests tests/OrigamiTests.cpp)
target_link_libraries(OrigamiTests PRIVATE origami_dsp)
set_target_properties(OrigamiTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(OrigamiTests PRIVATE -O2 -Wall -Wextra -Wpedantic -Wshadow -Werror)
endif()
add_test(NAME origami COMMAND OrigamiTests)

# ---- the panel (JUCE): shared by the plugin editor and the rack's ORIGAMI device. Hosts compile these sources
# into their own JUCE targets; include as "origami/plugin/OrigamiPanel.h". ----
set(ORIGAMI_UI_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/origami/plugin/OrigamiPanel.cpp
  CACHE INTERNAL "ORIGAMI panel sources for hosts that embed the ORIGAMI device")
set(ORIGAMI_PLUGIN_SOURCES
  src/origami/plugin/OrigamiProcessor.cpp
  src/origami/plugin/OrigamiEditor.cpp
  ${ORIGAMI_UI_SOURCES})

# ---- the standalone effect: VST3 + Standalone, stereo in/out plus an optional sidechain ----
juce_add_plugin(Origami
  COMPANY_NAME "Martial Systems"
  BUNDLE_ID com.martialsystems.origami
  PLUGIN_MANUFACTURER_CODE Mrsy
  PLUGIN_CODE Orgm
  FORMATS VST3 Standalone
  PRODUCT_NAME "ORIGAMI"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  VST3_CATEGORIES Fx Distortion
  COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(Origami PRIVATE ${ORIGAMI_PLUGIN_SOURCES})
target_compile_definitions(Origami PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(Origami PRIVATE origami_dsp juce::juce_audio_utils
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ---- plugin tests + headless screenshots (processor and editor without a host).
# Pass a folder as the first argument to write the PNGs: OrigamiPluginTests ./shots ----
juce_add_console_app(OrigamiPluginTests PRODUCT_NAME "OrigamiPluginTests")
target_sources(OrigamiPluginTests PRIVATE tests/OrigamiPluginTests.cpp ${ORIGAMI_PLUGIN_SOURCES})
target_compile_definitions(OrigamiPluginTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 ORIGAMI_NO_PLUGIN_ENTRY=1)
target_link_libraries(OrigamiPluginTests PRIVATE origami_dsp juce::juce_audio_utils juce::juce_recommended_config_flags)
add_test(NAME origami_plugin COMMAND OrigamiPluginTests)
