idf_component_register(
    SRCS "inference.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp-dl esp32-camera esp_new_jpeg coco_detect
) 