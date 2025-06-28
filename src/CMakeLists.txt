cmake_minimum_required(VERSION 3.20)

set(PICO_SDK_PATH /home/arajun/dev/pico-sdk)

set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(picoMusicPlayer C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

add_subdirectory(i2s/32Stereo build)

# Add executable. Default name is the project name, version 0.1
add_executable(picoMusicPlayer
    main.c
)
# Can leave these off for silent mode:
# add_compile_definitions(USE_PRINTF USE_DBG_PRINTF)
#add_compile_definitions(USE_PRINTF)

# Add the standard library and FatFS/SPI to the build
target_link_libraries(picoMusicPlayer 
    pico_stdlib
    hardware_pio
    hardware_timer
    hardware_clocks
    picoI2S32
)

pico_set_program_name(picoMusicPlayer "picoMusicPlayer")
pico_set_program_version(picoMusicPlayer "0.1")

# Choose source and destination for standard input and output:
#   See 4.1. Serial input and output on Raspberry Pi Pico in Getting started with Raspberry Pi Pico (https://datasheets.raspberrypi.org/pico/getting-started-with-pico.pdf)
#   and 2.7.1. Standard Input/Output (stdio) Support in Raspberry Pi Pico C/C++ SDK (https://datasheets.raspberrypi.org/pico/raspberry-pi-pico-c-sdk.pdf):
#pico_enable_stdio_uart(picoMusicPlayer 0)
pico_enable_stdio_usb(picoMusicPlayer 1)

pico_add_extra_outputs(picoMusicPlayer)
