add_library(i2s INTERFACE)

target_sources(i2s
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/i2s.c
)

pico_generate_pio_header(i2s ${CMAKE_CURRENT_LIST_DIR}/i2s16m.pio)
pico_generate_pio_header(i2s ${CMAKE_CURRENT_LIST_DIR}/i2s32s.pio)

target_include_directories(i2s INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_SOURCE_DIR}
)

target_link_libraries(i2s
  INTERFACE
    hardware_pio
    hardware_gpio
)