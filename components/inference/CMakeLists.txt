idf_component_register(
    SRCS "inference.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp-dl esp32-camera esp_new_jpeg human_face_detect
)

# Aggiungi il path di include per human_face_detect
target_include_directories(${COMPONENT_LIB} PRIVATE models/human_face_detect) 