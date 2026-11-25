# =============================================================================
#  AETHERFALL — immersive atmosphere & texture FX
#  Build: CMake >= 3.22, JUCE 8 (FetchContent or libs/JUCE), C++20
# =============================================================================
cmake_minimum_required(VERSION 3.22)

# macOS: minimum target 12.0. Universal builds are requested from the command
# line / CI with -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64".
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")

project(AETHERFALL VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Static MSVC runtime: the VST3 bundle does not depend on the VC++ redist.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# -----------------------------------------------------------------------------
# Options
# -----------------------------------------------------------------------------
option(AETHERFALL_BUILD_TESTS        "Build the unit tests (Catch2)"                    ON)
option(AETHERFALL_COPY_AFTER_BUILD   "Copy plug-ins to the system plug-in folders"      OFF)
option(AETHERFALL_ENABLE_LTO         "Enable link-time optimisation in Release"         OFF)
set(AETHERFALL_JUCE_DIR "" CACHE PATH "Local JUCE checkout (empty = libs/JUCE or FetchContent)")

set(AETHERFALL_JUCE_TAG    "8.0.17-security" CACHE STRING "JUCE git tag used by FetchContent")
set(AETHERFALL_CATCH2_TAG  "v3.16.1"         CACHE STRING "Catch2 git tag used by FetchContent")

include(FetchContent)

# -----------------------------------------------------------------------------
# JUCE
# Priority: AETHERFALL_JUCE_DIR > libs/JUCE (submodule/copy) > FetchContent.
# -----------------------------------------------------------------------------
if (AETHERFALL_JUCE_DIR)
    add_subdirectory("${AETHERFALL_JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/JUCE/CMakeLists.txt")
    add_subdirectory(libs/JUCE EXCLUDE_FROM_ALL)
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${AETHERFALL_JUCE_TAG}
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# -----------------------------------------------------------------------------
# Plug-in target
# -----------------------------------------------------------------------------
set(AETHERFALL_FORMATS VST3 Standalone)
if (APPLE)
    list(APPEND AETHERFALL_FORMATS AU)
endif()

juce_add_plugin(Aetherfall
    PRODUCT_NAME                 "AETHERFALL"
    VERSION                      ${PROJECT_VERSION}
    COMPANY_NAME                 "Aetherfall Audio"
    BUNDLE_ID                    "com.aetherfallaudio.aetherfall"
    DESCRIPTION                  "Immersive atmosphere and synthesized texture processor"
    PLUGIN_MANUFACTURER_CODE     Aefl
    PLUGIN_CODE                  Afx1
    FORMATS                      ${AETHERFALL_FORMATS}
    IS_SYNTH                     FALSE
    NEEDS_MIDI_INPUT             TRUE      # optional MIDI modulation (FL can route MIDI to FX)
    NEEDS_MIDI_OUTPUT            FALSE
    IS_MIDI_EFFECT               FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS  FALSE
    VST3_CATEGORIES              Fx Reverb
    AU_MAIN_TYPE                 kAudioUnitType_MusicEffect
    AU_SANDBOX_SAFE              TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT   "AETHERFALL Standalone processes live audio input."
    COPY_PLUGIN_AFTER_BUILD      ${AETHERFALL_COPY_AFTER_BUILD})

# -----------------------------------------------------------------------------
# Sources (globbed so new modules added in later phases are picked up;
# CONFIGURE_DEPENDS re-runs the glob at build time).
# -----------------------------------------------------------------------------
file(GLOB_RECURSE AETHERFALL_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h")

# Everything shared by the plug-in and the test runner lives in this
# INTERFACE library, so both targets compile the exact same code.
add_library(AetherfallShared INTERFACE)

target_include_directories(AetherfallShared INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source")

target_compile_definitions(AetherfallShared INTERFACE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    AETHERFALL_VERSION_STRING="${PROJECT_VERSION}")

target_link_libraries(AetherfallShared INTERFACE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

if (AETHERFALL_ENABLE_LTO)
    target_link_libraries(AetherfallShared INTERFACE juce::juce_recommended_lto_flags)
endif()

target_sources(Aetherfall PRIVATE ${AETHERFALL_SOURCES})
target_link_libraries(Aetherfall PRIVATE AetherfallShared)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Source" PREFIX "Source" FILES ${AETHERFALL_SOURCES})

# -----------------------------------------------------------------------------
# Tests
# -----------------------------------------------------------------------------
if (AETHERFALL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
