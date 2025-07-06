add_executable(main
    main.c
    hw_config.c
    play.c
    wav.c
)
target_link_libraries(main 
    pico_stdlib
    hardware_pio
    hardware_gpio
    hardware_timer
    hardware_clocks
    hardware_dma
    hardware_irq
    i2s
    no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
)

target_include_directories(main PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

pico_enable_stdio_usb(main 1)

pico_add_extra_outputs(main)
