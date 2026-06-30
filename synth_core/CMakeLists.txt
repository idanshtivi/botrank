cmake_minimum_required(VERSION 3.16)
project(MiniMoogCore VERSION 0.1.0 LANGUAGES CXX)

# ── Language standard ─────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MINIMOOG_USE_TRUSTED_BIN "Place test/tool executables in a trusted directory for local Windows execution." ON)
option(SYNTH_CORE_BUILD_TESTS "Build SynthCore unit tests." ON)
set(MINIMOOG_TRUSTED_BIN_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../trusted_bin"
    CACHE PATH
    "Trusted output directory for local test/tool executables."
)

# ── Strict warnings ───────────────────────────────────────────────────────────
if(MSVC)
    add_compile_options(/W4 /WX /permissive-)
else()
    add_compile_options(
        -Wall -Wextra -Wpedantic -Werror
        -Wno-unused-parameter          # stubs have intentional unused params
    )
endif()

# ── Poly diagnostic logger (compile-time gate) ────────────────────────────────
option(LADDERVOICE_ENABLE_POLY_TRACE
    "Enable live poly-crackle diagnostic tracing (writes CSV files at runtime when LADDERVOICE_POLY_TRACE=1 env var is set)"
    OFF)

# ── Core library (header-only interface + compiled sources) ───────────────────
add_library(SynthCoreLib STATIC
    Source/SynthEngine.cpp
    Source/SynthVoice.cpp
    Source/VoiceController.cpp
    Source/Oscillator.cpp
    Source/OscillatorBank.cpp
    Source/NoiseGenerator.cpp
    Source/Mixer.cpp
    Source/ContourGenerator.cpp
    Source/LadderFilter.cpp
    Source/Filter.cpp
    Source/Envelope.cpp
    Source/Vca.cpp
    Source/OutputStage.cpp
    Source/DSPUtils.cpp
    Source/PolyTraceLogger.cpp
)

if(LADDERVOICE_ENABLE_POLY_TRACE)
    target_compile_definitions(SynthCoreLib PUBLIC LADDERVOICE_ENABLE_POLY_TRACE=1)
    message(STATUS "PolyTraceLogger: ENABLED (runtime gate: env LADDERVOICE_POLY_TRACE=1)")
else()
    message(STATUS "PolyTraceLogger: disabled (pass -DLADDERVOICE_ENABLE_POLY_TRACE=ON to enable)")
endif()

target_include_directories(SynthCoreLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Include
)

# ── Google Test ───────────────────────────────────────────────────────────────
if(SYNTH_CORE_BUILD_TESTS)
include(FetchContent)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG f8d7d77c06936315286eb55f8de22cd23c188571 # v1.14.0
)
FetchContent_MakeAvailable(googletest)

enable_testing()
endif()

# ── WAV render demo ───────────────────────────────────────────────────────────
add_executable(WavRender
    Tools/wav_render.cpp
)
target_link_libraries(WavRender PRIVATE SynthCoreLib)

# ── Audio safety diagnostic tool ─────────────────────────────────────────────
add_executable(AudioSafetyCheck
    Tools/audio_safety_check.cpp
)
target_link_libraries(AudioSafetyCheck PRIVATE SynthCoreLib)

# ── Test executable ───────────────────────────────────────────────────────────
if(SYNTH_CORE_BUILD_TESTS)
add_executable(SynthCoreTests
    Tests/InfrastructureTests.cpp
    Tests/VoiceControllerTests.cpp
    Tests/DspTests.cpp
    Tests/AnalysisTests.cpp
    Tests/AudioOutputTests.cpp
    Tests/OscillatorTests.cpp
    Tests/OscillatorBankTests.cpp
    Tests/NoiseGeneratorTests.cpp
    Tests/MixerTests.cpp
    Tests/LadderFilterTests.cpp
    Tests/SynthEngineAudioTests.cpp
    Tests/ContourGeneratorTests.cpp
    Tests/VCATests.cpp
    Tests/OutputStageTests.cpp
    Tests/LfoTests.cpp
    Tests/ControlCompletionTests.cpp
)

target_link_libraries(SynthCoreTests
    PRIVATE
        SynthCoreLib
        GTest::gtest_main
)
endif()

if(MINIMOOG_USE_TRUSTED_BIN)
    file(MAKE_DIRECTORY "${MINIMOOG_TRUSTED_BIN_DIR}")
    set(trusted_bin_targets WavRender AudioSafetyCheck)
    if(SYNTH_CORE_BUILD_TESTS)
        list(APPEND trusted_bin_targets SynthCoreTests)
    endif()
    foreach(target IN LISTS trusted_bin_targets)
        set_target_properties(${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "$<1:${MINIMOOG_TRUSTED_BIN_DIR}>"
        )
    endforeach()
endif()

if(SYNTH_CORE_BUILD_TESTS)
include(GoogleTest)
gtest_discover_tests(SynthCoreTests
    DISCOVERY_TIMEOUT 30
    DISCOVERY_MODE    PRE_TEST
)
endif()
