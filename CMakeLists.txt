cmake_minimum_required(VERSION 3.25)

project(LuckyCompressor VERSION 1.0.0 LANGUAGES C CXX)

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

option(LUCKY_COMPRESSOR_INSTALL_AFTER_BUILD "Copy the plug-ins into ~/Library/Audio/Plug-Ins after building" OFF)

juce_add_plugin(LuckyCompressor
    COMPANY_NAME "Lucky Labs"
    BUNDLE_ID com.luckylabs.luckycompressor
    PLUGIN_MANUFACTURER_CODE Lcky
    PLUGIN_CODE Lcmp
    FORMATS VST3 AU
    PRODUCT_NAME "Lucky Compressor"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD ${LUCKY_COMPRESSOR_INSTALL_AFTER_BUILD}
)

juce_add_binary_data(LuckyCompressorSamples
    HEADER_NAME LuckyCompressorSamples.h
    NAMESPACE LuckyCompressorSamples
    SOURCES Resources/TestSnare.wav)

set(LUCKY_COMPRESSOR_CORE_SOURCES
    Source/dsp/Modes.cpp
    Source/dsp/Compressor.cpp
    Source/dsp/Colour.cpp
    Source/dsp/LevelMatcher.cpp
    Source/dsp/CompressorEngine.cpp
    Source/dsp/ExampleHit.cpp
)

target_sources(LuckyCompressor PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/LiveView.cpp
    Source/ui/RevealPrompt.cpp
    Source/ui/HitView.cpp
    Source/ui/Tips.cpp
    ${LUCKY_COMPRESSOR_CORE_SOURCES}
)

target_include_directories(LuckyCompressor PRIVATE Source)

target_compile_definitions(LuckyCompressor PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LuckyCompressor
    PRIVATE
        lucky_ui
        lucky_dsp
        LuckyLabsFonts
        LuckyCompressorSamples
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_console_app(LuckyCompressorTests PRODUCT_NAME "Lucky Compressor Tests")

target_sources(LuckyCompressorTests PRIVATE
    Tests/TestMain.cpp
    Tests/ModesTests.cpp
    Tests/CompressorTests.cpp
    Tests/LevelMatcherTests.cpp
    Tests/EngineTests.cpp
    Tests/ExampleHitTests.cpp
    Tests/ColourTests.cpp
    ${LUCKY_COMPRESSOR_CORE_SOURCES}
)

target_include_directories(LuckyCompressorTests PRIVATE Source Tests)

target_compile_definitions(LuckyCompressorTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_UNIT_TESTS=1
)

target_link_libraries(LuckyCompressorTests
    PRIVATE
        lucky_dsp
        LuckyCompressorSamples
        juce::juce_audio_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

enable_testing()
add_test(NAME LuckyCompressorTests COMMAND $<TARGET_FILE:LuckyCompressorTests>)

add_executable(LuckyCompressorProcessorTests Tests/ProcessorTests.cpp)
target_include_directories(LuckyCompressorProcessorTests PRIVATE Source Tests $<TARGET_PROPERTY:LuckyCompressor,INCLUDE_DIRECTORIES>)
target_compile_definitions(LuckyCompressorProcessorTests PRIVATE JUCE_UNIT_TESTS=1 $<TARGET_PROPERTY:LuckyCompressor,COMPILE_DEFINITIONS>)
target_link_libraries(LuckyCompressorProcessorTests PRIVATE LuckyCompressor)
add_test(NAME LuckyCompressorProcessorTests COMMAND $<TARGET_FILE:LuckyCompressorProcessorTests>)

add_executable(LuckyCompressorSnapshot tools/Snapshot.cpp)
target_include_directories(LuckyCompressorSnapshot PRIVATE Source Tests $<TARGET_PROPERTY:LuckyCompressor,INCLUDE_DIRECTORIES>)
target_compile_definitions(LuckyCompressorSnapshot PRIVATE $<TARGET_PROPERTY:LuckyCompressor,COMPILE_DEFINITIONS>)
target_link_libraries(LuckyCompressorSnapshot PRIVATE LuckyCompressor)

juce_add_console_app(LuckyCompressorMeasure PRODUCT_NAME "Lucky Compressor Measure")
target_sources(LuckyCompressorMeasure PRIVATE tools/Measure.cpp)
target_compile_definitions(LuckyCompressorMeasure PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_AU=1)
target_link_libraries(LuckyCompressorMeasure
    PRIVATE juce::juce_audio_processors juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

add_executable(LuckyCompressorBench tools/Benchmark.cpp)
target_include_directories(LuckyCompressorBench PRIVATE Source Tests $<TARGET_PROPERTY:LuckyCompressor,INCLUDE_DIRECTORIES>)
target_compile_definitions(LuckyCompressorBench PRIVATE $<TARGET_PROPERTY:LuckyCompressor,COMPILE_DEFINITIONS>)
target_link_libraries(LuckyCompressorBench PRIVATE LuckyCompressor)
