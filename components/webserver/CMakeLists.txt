idf_component_register(SRCS "webserver.cpp" "camera.cpp"
                    INCLUDE_DIRS "."
                    EMBED_FILES "main_page.html"
                    REQUIRES esp_http_server esp32-camera inference)