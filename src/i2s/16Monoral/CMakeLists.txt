add_library(picoI2S INTERFACE)


pico_generate_pio_header(picoI2S ${CMAKE_CURRENT_LIST_DIR}/i2s.pio)

target_include_directories(picoI2S INTERFACE
  &{CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(picoI2S
  INTERFACE
    hardware_pio
)