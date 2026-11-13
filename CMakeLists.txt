cmake_minimum_required(VERSION 3.25)

project(LuckyComp VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
set(CMAKE_OSX_DEPLOYMENT_TARGET 13.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

set(LUCKY_LABS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../lucky-labs" CACHE PATH "Lucky Labs shared code")
include("${LUCKY_LABS_DIR}/cmake/LuckyLabs.cmake")

option(LUCKY_COMP_INSTALL_AFTER_BUILD "Copy the plug-ins into ~/Library/Audio/Plug-Ins after building" OFF)

juce_add_plugin(LuckyComp
    COMPANY_NAME "Lucky Labs"
    BUNDLE_ID com.luckylabs.luckycomp
    PLUGIN_MANUFACTURER_CODE Lcky
    PLUGIN_CODE Lcmp
    FORMATS VST3 AU
    PRODUCT_NAME "Lucky Comp"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD ${LUCKY_COMP_INSTALL_AFTER_BUILD}
)

juce_add_binary_data(LuckyCompSamples
    HEADER_NAME LuckyCompSamples.h
    NAMESPACE LuckyCompSamples
    SOURCES Resources/TestSnare.wav)

set(LUCKY_COMP_CORE_SOURCES
    Source/dsp/Modes.cpp
    Source/dsp/Compressor.cpp
    Source/dsp/Colour.cpp
    Source/dsp/LevelMatcher.cpp
    Source/dsp/CompressorEngine.cpp
    Source/dsp/ExampleHit.cpp
)

target_sources(LuckyComp PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/LiveView.cpp
    Source/ui/RevealPrompt.cpp
    Source/ui/HitView.cpp
    Source/ui/Tips.cpp
    ${LUCKY_COMP_CORE_SOURCES}
)

target_include_directories(LuckyComp PRIVATE Source)

target_compile_definitions(LuckyComp PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LuckyComp
    PRIVATE
        lucky_ui
        lucky_dsp
        LuckyLabsFonts
        LuckyCompSamples
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_console_app(LuckyCompTests PRODUCT_NAME "Lucky Comp Tests")

target_sources(LuckyCompTests PRIVATE
    Tests/TestMain.cpp
    Tests/ModesTests.cpp
    Tests/CompressorTests.cpp
    Tests/LevelMatcherTests.cpp
    Tests/EngineTests.cpp
    Tests/ExampleHitTests.cpp
    Tests/ColourTests.cpp
    ${LUCKY_COMP_CORE_SOURCES}
)

target_include_directories(LuckyCompTests PRIVATE Source Tests)

target_compile_definitions(LuckyCompTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_UNIT_TESTS=1
)

target_link_libraries(LuckyCompTests
    PRIVATE
        lucky_dsp
        LuckyCompSamples
        juce::juce_audio_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

enable_testing()
add_test(NAME LuckyCompTests COMMAND $<TARGET_FILE:LuckyCompTests>)

add_executable(LuckyCompProcessorTests Tests/ProcessorTests.cpp)
target_include_directories(LuckyCompProcessorTests PRIVATE Source Tests $<TARGET_PROPERTY:LuckyComp,INCLUDE_DIRECTORIES>)
target_compile_definitions(LuckyCompProcessorTests PRIVATE JUCE_UNIT_TESTS=1 $<TARGET_PROPERTY:LuckyComp,COMPILE_DEFINITIONS>)
target_link_libraries(LuckyCompProcessorTests PRIVATE LuckyComp)
add_test(NAME LuckyCompProcessorTests COMMAND $<TARGET_FILE:LuckyCompProcessorTests>)

add_executable(LuckyCompSnapshot tools/Snapshot.cpp)
target_include_directories(LuckyCompSnapshot PRIVATE Source Tests $<TARGET_PROPERTY:LuckyComp,INCLUDE_DIRECTORIES>)
target_compile_definitions(LuckyCompSnapshot PRIVATE $<TARGET_PROPERTY:LuckyComp,COMPILE_DEFINITIONS>)
target_link_libraries(LuckyCompSnapshot PRIVATE LuckyComp)

juce_add_console_app(LuckyCompMeasure PRODUCT_NAME "Lucky Comp Measure")
target_sources(LuckyCompMeasure PRIVATE tools/Measure.cpp)
target_compile_definitions(LuckyCompMeasure PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_AU=1)
target_link_libraries(LuckyCompMeasure
    PRIVATE juce::juce_audio_processors juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

add_executable(LuckyCompBench tools/Benchmark.cpp)
target_include_directories(LuckyCompBench PRIVATE Source Tests $<TARGET_PROPERTY:LuckyComp,INCLUDE_DIRECTORIES>)
target_compile_definitions(LuckyCompBench PRIVATE $<TARGET_PROPERTY:LuckyComp,COMPILE_DEFINITIONS>)
target_link_libraries(LuckyCompBench PRIVATE LuckyComp)
