#include "inference.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "dl_image.hpp"
#include "human_face_detect.hpp"
#include <string.h>

static const char* TAG = "INFERENCE";

// Statistiche globali
static inference_stats_t stats = {0};
static bool initialized = false;
static HumanFaceDetect* face_detector = nullptr;

bool inference_init(void) {
    ESP_LOGI(TAG, "Inizializzazione sistema di inferenza HumanFaceDetect...");
    
    if (initialized) {
        ESP_LOGW(TAG, "Sistema già inizializzato");
        return true;
    }
    
    // Crea il detector per face detection
    face_detector = new HumanFaceDetect();
    if (!face_detector) {
        ESP_LOGE(TAG, "Errore creazione HumanFaceDetect");
        return false;
    }
    
    // Reset statistiche
    memset(&stats, 0, sizeof(stats));
    
    initialized = true;
    ESP_LOGI(TAG, "Sistema di inferenza HumanFaceDetect inizializzato con successo");
    return true;
}

bool inference_process_image(const uint8_t* jpeg_data, size_t jpeg_size, inference_result_t* result) {
    if (!initialized || !jpeg_data || !result || !face_detector) {
        ESP_LOGE(TAG, "Parametri non validi o sistema non inizializzato");
        return false;
    }
    
    ESP_LOGI(TAG, "Elaborazione immagine JPEG (%zu bytes) con HumanFaceDetect", jpeg_size);
    
    uint32_t start_time = esp_timer_get_time() / 1000; // Converti in ms
    uint32_t free_heap_before = esp_get_free_heap_size();
    
    // Prepara struttura JPEG per ESP-DL
    dl::image::jpeg_img_t jpeg_img = {
        .data = (void*)jpeg_data,
        .data_len = jpeg_size
    };
    
    // Decodifica JPEG usando ESP-DL
    auto img = sw_decode_jpeg(jpeg_img, dl::image::DL_IMAGE_PIX_TYPE_RGB888);
    if (!img.data) {
        ESP_LOGE(TAG, "Errore decodifica JPEG");
        return false;
    }
    
    ESP_LOGI(TAG, "Immagine decodificata: %dx%d", img.width, img.height);
    
    // Esegui face detection
    bool face_detected = false;
    float max_confidence = 0.0f;
    
    if (img.data && img.width > 0 && img.height > 0) {
        auto &detect_results = face_detector->run(img);
        
        // Controlla se sono state rilevate facce
        for (const auto &res : detect_results) {
            ESP_LOGI(TAG, "Faccia rilevata: score=%.3f, box=[%d,%d,%d,%d]", 
                     res.score, res.box[0], res.box[1], res.box[2], res.box[3]);
            
            if (res.score > max_confidence) {
                max_confidence = res.score;
                face_detected = true;
            }
        }
        
        if (!face_detected) {
            ESP_LOGI(TAG, "Nessuna faccia rilevata");
        }
    }
    
    // Libera memoria immagine
    heap_caps_free(img.data);
    
    uint32_t end_time = esp_timer_get_time() / 1000;
    uint32_t free_heap_after = esp_get_free_heap_size();
    
    // Popola il risultato
    result->face_detected = face_detected;
    result->confidence = max_confidence;
    result->inference_time_ms = end_time - start_time;
    result->memory_used_kb = (free_heap_before - free_heap_after) / 1024;
    
    // Aggiorna statistiche
    stats.total_inferences++;
    stats.avg_inference_time_ms = 
        (stats.avg_inference_time_ms * (stats.total_inferences - 1) + result->inference_time_ms) / 
        stats.total_inferences;
    
    if (result->memory_used_kb > stats.max_memory_used_kb) {
        stats.max_memory_used_kb = result->memory_used_kb;
    }
    
    ESP_LOGI(TAG, "Inferenza completata: %s (conf: %.2f, tempo: %dms, mem: %dKB)", 
             result->face_detected ? "FACCIA" : "NO_FACCIA",
             result->confidence,
             result->inference_time_ms,
             result->memory_used_kb);
    
    return true;
}

void inference_get_stats(inference_stats_t* result_stats) {
    if (result_stats) {
        memcpy(result_stats, &stats, sizeof(inference_stats_t));
    }
}

void inference_deinit(void) {
    if (!initialized) {
        return;
    }
    
    ESP_LOGI(TAG, "Deinizializzazione sistema di inferenza HumanFaceDetect...");
    
    if (face_detector) {
        delete face_detector;
        face_detector = nullptr;
    }
    
    initialized = false;
    ESP_LOGI(TAG, "Sistema di inferenza HumanFaceDetect deinizializzato");
} 