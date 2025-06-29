cmake_minimum_required(VERSION 3.20)

set(PICO_SDK_PATH /home/arajun/dev/pico-sdk)

set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(i2sTestSine C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

add_subdirectory(../../src/i2s build)

# Add executable. Default name is the project name, version 0.1
add_executable(i2sTestSine 
    sine.c
)
# Can leave these off for silent mode:
# add_compile_definitions(USE_PRINTF USE_DBG_PRINTF)
#add_compile_definitions(USE_PRINTF)

# Add the standard library and FatFS/SPI to the build
target_link_libraries(i2sTestSine 
    pico_stdlib
    hardware_pio
    hardware_timer
    hardware_clocks
    i2s
)

pico_set_program_name(i2sTestSine "i2sTestSine")
pico_set_program_version(i2sTestSine "0.1")

# Choose source and destination for standard input and output:
#   See 4.1. Serial input and output on Raspberry Pi Pico in Getting started with Raspberry Pi Pico (https://datasheets.raspberrypi.org/pico/getting-started-with-pico.pdf)
#   and 2.7.1. Standard Input/Output (stdio) Support in Raspberry Pi Pico C/C++ SDK (https://datasheets.raspberrypi.org/pico/raspberry-pi-pico-c-sdk.pdf):
#pico_enable_stdio_uart(i2sTestSine 0)
pico_enable_stdio_usb(i2sTestSine 1)

pico_add_extra_outputs(i2sTestSine)
