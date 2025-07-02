idf_component_register(SRCS "webserver.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_http_server esp32-camera led_control) 