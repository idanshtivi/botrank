cmake_minimum_required(VERSION 3.22)
project(LadderVoiceWorkspace VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(JUCE_DIR "C:/Dev/Libraries/JUCE" CACHE PATH "Path to local JUCE checkout")

add_subdirectory("${JUCE_DIR}" JUCE)
set(SYNTH_CORE_BUILD_TESTS OFF CACHE BOOL "Build SynthCore unit tests." FORCE)
add_subdirectory(synth_core)
add_subdirectory(plugin)
