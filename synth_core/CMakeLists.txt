cmake_minimum_required(VERSION 3.16)
project(MiniMoogCore VERSION 0.1.0 LANGUAGES CXX)

# ── Language standard ─────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Strict warnings ───────────────────────────────────────────────────────────
if(MSVC)
    add_compile_options(/W4 /WX /permissive-)
else()
    add_compile_options(
        -Wall -Wextra -Wpedantic -Werror
        -Wno-unused-parameter          # stubs have intentional unused params
    )
endif()

# ── Core library (header-only interface + compiled sources) ───────────────────
add_library(SynthCoreLib STATIC
    Source/SynthEngine.cpp
    Source/VoiceController.cpp
    Source/Oscillator.cpp
    Source/Mixer.cpp
    Source/Filter.cpp
    Source/Envelope.cpp
    Source/Vca.cpp
    Source/OutputStage.cpp
    Source/DSPUtils.cpp
)

target_include_directories(SynthCoreLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Include
)

# ── Google Test ───────────────────────────────────────────────────────────────
find_package(GTest REQUIRED)

enable_testing()

# ── Test executable ───────────────────────────────────────────────────────────
add_executable(SynthCoreTests
    Tests/InfrastructureTests.cpp
    Tests/DspTests.cpp
    Tests/AnalysisTests.cpp
    Tests/OscillatorTests.cpp
)

target_link_libraries(SynthCoreTests
    PRIVATE
        SynthCoreLib
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(SynthCoreTests)
