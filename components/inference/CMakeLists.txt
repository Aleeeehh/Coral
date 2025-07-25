idf_component_register(
    SRCS "inference.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp-dl esp32-camera esp_new_jpeg human_face_detect monitor esp-tflite-micro
               EMBED_FILES "models/yolo11n_float32.tflite"
)

# Aggiungi il path di include per human_face_detect
target_include_directories(${COMPONENT_LIB} PRIVATE models/human_face_detect)

# Aggiungi gli include di TensorFlow Lite
target_include_directories(${COMPONENT_LIB} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/models
) 