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

include("${catch2_SOURCE_DIR}/extras/Catch.cmake")
catch_discover_tests(AetherfallTests
    DISCOVERY_MODE PRE_TEST
    PROPERTIES TIMEOUT 300)
