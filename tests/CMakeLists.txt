# =============================================================================
#  AETHERFALL unit tests (Catch2 v3)
#  The test runner compiles the same sources as the plug-in (via the
#  AetherfallShared interface library) and drives the DSP headlessly.
# =============================================================================
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        ${AETHERFALL_CATCH2_TAG}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE)
FetchContent_MakeAvailable(Catch2)

juce_add_console_app(AetherfallTests
    PRODUCT_NAME "AetherfallTests")

file(GLOB AETHERFALL_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

target_sources(AetherfallTests PRIVATE ${AETHERFALL_SOURCES} ${AETHERFALL_TEST_SOURCES})

target_include_directories(AetherfallTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(AetherfallTests PRIVATE
    AetherfallShared
    Catch2::Catch2)

# The plug-in sources use JucePlugin_* macros only through PluginInfo.h,
# which provides fallbacks, so no extra definitions are needed here.
target_compile_definitions(AetherfallTests PRIVATE AETHERFALL_TESTS=1)

# Tests compare floats exactly on purpose (e.g. smoother end values).
# Source-level options come after the interface warning flags, so they win.
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(${AETHERFALL_TEST_SOURCES} PROPERTIES COMPILE_OPTIONS "-Wno-float-equal")
endif()

include("${catch2_SOURCE_DIR}/extras/Catch.cmake")
catch_discover_tests(AetherfallTests
    DISCOVERY_MODE PRE_TEST
    PROPERTIES TIMEOUT 300)

# -----------------------------------------------------------------------------
# Host-level tests: load the built VST3 through the VST3 API like a DAW.
# -----------------------------------------------------------------------------
juce_add_console_app(AetherfallHostTests PRODUCT_NAME "AetherfallHostTests")
target_sources(AetherfallHostTests PRIVATE host/HostMain.cpp host/HostTests.cpp)
get_target_property(AETHERFALL_VST3_ARTEFACT Aetherfall_VST3 JUCE_PLUGIN_ARTEFACT_FILE)
target_compile_definitions(AetherfallHostTests PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    AETHERFALL_VST3_PATH="${AETHERFALL_VST3_ARTEFACT}")
target_link_libraries(AetherfallHostTests PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_recommended_config_flags
    Catch2::Catch2)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(AetherfallHostTests PRIVATE -Wno-float-equal)
endif()
add_dependencies(AetherfallHostTests Aetherfall_VST3)

catch_discover_tests(AetherfallHostTests
    DISCOVERY_MODE PRE_TEST
    TEST_PREFIX "host: "
    PROPERTIES TIMEOUT 600)
