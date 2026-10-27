# =============================================================================
#  TWISTED TIMESTRETCH — Akaizer as an instrument plugin
#  CMake build for JUCE 8 (AU + VST3 + Standalone on macOS; VST3 elsewhere)
# =============================================================================
cmake_minimum_required(VERSION 3.22)

project(TwistedTimestretch VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Apple Silicon first, but build universal binaries so Rosetta hosts still work.
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

option(TT_BUILD_TESTS        "Build the headless test runner"                       ON)
option(TT_COPY_AFTER_BUILD   "Copy plugins into ~/Library/Audio/Plug-Ins after build" OFF)
option(TT_MINIMAL_LINUX_DEPS "Linux CI only: disable optional X11/ALSA features"     OFF)
set(TT_JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout (leave empty to download JUCE 8.0.4)")

# -----------------------------------------------------------------------------
#  JUCE dependency
# -----------------------------------------------------------------------------
if(TT_JUCE_PATH)
    add_subdirectory(${TT_JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# -----------------------------------------------------------------------------
#  Plugin target
# -----------------------------------------------------------------------------
set(TT_FORMATS VST3)
if(APPLE)
    list(APPEND TT_FORMATS AU Standalone)
endif()

juce_add_plugin(TwistedTimestretch
    PRODUCT_NAME                "TwistedTimestretch"
    COMPANY_NAME                "Twisted Genetics"
    BUNDLE_ID                   "com.twistedgenetics.twistedtimestretch"
    PLUGIN_MANUFACTURER_CODE    Twgn
    PLUGIN_CODE                 Twts
    FORMATS                     ${TT_FORMATS}
    IS_SYNTH                    TRUE        # an instrument: MIDI in, audio out
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Instrument Sampler
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    MICROPHONE_PERMISSION_ENABLED FALSE
    COPY_PLUGIN_AFTER_BUILD     ${TT_COPY_AFTER_BUILD})

juce_generate_juce_header(TwistedTimestretch)

include(cmake/Sources.cmake)
target_sources(TwistedTimestretch PRIVATE ${TT_PLUGIN_SOURCES})
target_include_directories(TwistedTimestretch PRIVATE Source)

set(TT_JUCE_DEFINES
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

if(TT_MINIMAL_LINUX_DEPS)
    list(APPEND TT_JUCE_DEFINES
        JUCE_ALSA=0 JUCE_JACK=0
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XSHM=0
        JUCE_USE_XRENDER=0 JUCE_USE_XCURSOR=0)
endif()

target_compile_definitions(TwistedTimestretch PUBLIC ${TT_JUCE_DEFINES})

target_link_libraries(TwistedTimestretch
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# -----------------------------------------------------------------------------
#  Headless test runner (no DAW needed)
# -----------------------------------------------------------------------------
if(TT_BUILD_TESTS)
    juce_add_console_app(TwistedTimestretchTests PRODUCT_NAME "TwistedTimestretchTests")
    target_sources(TwistedTimestretchTests PRIVATE Tests/TestMain.cpp ${TT_PLUGIN_SOURCES})
    target_include_directories(TwistedTimestretchTests PRIVATE Source)
    target_compile_definitions(TwistedTimestretchTests PRIVATE
        ${TT_JUCE_DEFINES}
        JucePlugin_Name="TwistedTimestretch"
        JucePlugin_IsSynth=1
        JucePlugin_WantsMidiInput=1
        JucePlugin_ProducesMidiOutput=0
        JucePlugin_IsMidiEffect=0
        JucePlugin_VersionString="${PROJECT_VERSION}"
        JUCE_MODAL_LOOPS_PERMITTED=1
        TT_REFERENCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Tests/reference")
    target_link_libraries(TwistedTimestretchTests PRIVATE
        juce::juce_audio_utils juce::juce_audio_formats
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    juce_generate_juce_header(TwistedTimestretchTests)

    # Loads the BUILT plugin through JUCE's plugin hosting, like a DAW does.
    juce_add_console_app(TwistedTimestretchHostTest PRODUCT_NAME "TwistedTimestretchHostTest")
    target_sources(TwistedTimestretchHostTest PRIVATE Tests/HostTest.cpp)
    target_compile_definitions(TwistedTimestretchHostTest PRIVATE
        ${TT_JUCE_DEFINES} JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1
        $<$<PLATFORM_ID:Darwin>:JUCE_PLUGINHOST_AU=1>)
    target_link_libraries(TwistedTimestretchHostTest PRIVATE
        juce::juce_audio_utils juce::juce_audio_processors
        juce::juce_recommended_config_flags)
    juce_generate_juce_header(TwistedTimestretchHostTest)
    add_dependencies(TwistedTimestretchHostTest TwistedTimestretch_VST3)
endif()
