add_library(picoI2S32 INTERFACE)


pico_generate_pio_header(picoI2S32 ${CMAKE_CURRENT_LIST_DIR}/i2s32.pio)

target_include_directories(picoI2S32 INTERFACE
  &{CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(picoI2S32
  INTERFACE
    hardware_pio
)