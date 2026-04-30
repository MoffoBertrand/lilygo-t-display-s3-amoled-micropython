add_library(usermod_amoled_qspi INTERFACE)

target_sources(usermod_amoled_qspi INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/amoled_qspi_bus.c
)

target_include_directories(usermod_amoled_qspi INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${IDF_PATH}/components/esp_lcd/include
    ${IDF_PATH}/components/esp_driver_spi/include
    ${IDF_PATH}/components/esp_driver_gpio/include
    ${IDF_PATH}/components/soc/include
    ${IDF_PATH}/components/soc/esp32s3/include
    ${IDF_PATH}/components/hal/include
    ${IDF_PATH}/components/esp_common/include
    ${IDF_PATH}/components/esp_hw_support/include
)

target_link_libraries(usermod INTERFACE usermod_amoled_qspi)
