idf_component_register(
    SRCS "monitor.cpp"
    INCLUDE_DIRS "include"
    REQUIRES freertos esp_system esp_timer spi_flash esp_partition
) 