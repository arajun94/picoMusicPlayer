cmake_minimum_required(VERSION 3.20)

set(PICO_SDK_PATH /home/arajun/dev/pico-sdk)

set(PICO_BOARD pico2 CACHE STRING "Board type")

set(OPTIMIZATION_FLAG "-O3")

# Pull in Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(picoMusicPlayer C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/src)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib)