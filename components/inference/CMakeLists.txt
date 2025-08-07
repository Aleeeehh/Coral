idf_build_get_property(component_targets __COMPONENT_TARGETS)
if ("___idf_espressif__esp-dl" IN_LIST component_targets)
    idf_component_get_property(espdl_dir espressif__esp-dl COMPONENT_DIR)
elseif("___idf_esp-dl" IN_LIST component_targets)
    idf_component_get_property(espdl_dir esp-dl COMPONENT_DIR)
endif()
set(cmake_dir ${espdl_dir}/fbs_loader/cmake)
include(${cmake_dir}/utilities.cmake)
set(embed_files yolo11n.espdl)  # Cambiato da models/yolo11n.espdl

idf_component_register(
    SRCS "inference.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp-dl esp32-camera esp_new_jpeg human_face_detect monitor esp-tflite-micro
)
target_add_aligned_binary_data(${COMPONENT_LIB} ${embed_files} BINARY)

# Aggiungi il path di include per human_face_detect
target_include_directories(${COMPONENT_LIB} PRIVATE models/human_face_detect)

# Aggiungi gli include di TensorFlow Lite
target_include_directories(${COMPONENT_LIB} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/models
)