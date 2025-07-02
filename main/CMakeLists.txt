idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES webserver led_control esp32-camera esp_wifi nvs_flash) 