#include "inference.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "dl_image.hpp"
#include "coco_detect.hpp"
#include <string.h>

static const char* TAG = "INFERENCE";

// Statistiche globali
static inference_stats_t g_stats = {0};
static bool g_initialized = false;

// COCODetect gestisce internamente i buffer

bool inference_init(void) {
    ESP_LOGI(TAG, "Inizializzazione sistema di inferenza...");
    
    if (g_initialized) {
        ESP_LOGW(TAG, "Sistema già inizializzato");
        return true;
    }
    
    // COCODetect gestisce internamente i buffer, non serve allocare nulla
        
    // Reset statistiche
    memset(&g_stats, 0, sizeof(g_stats));
    
    g_initialized = true;
    ESP_LOGI(TAG, "Sistema di inferenza inizializzato con successo");
    return true;
}

bool inference_process_image(const uint8_t* jpeg_data, size_t jpeg_size, inference_result_t* result) {
    if (!g_initialized || !jpeg_data || !result) {
        ESP_LOGE(TAG, "Parametri non validi o sistema non inizializzato");
        return false;
    }
    
    ESP_LOGI(TAG, "Elaborazione immagine JPEG (%zu bytes)", jpeg_size);
    
    uint32_t start_time = esp_timer_get_time() / 1000; // Converti in ms
    uint32_t free_heap_before = esp_get_free_heap_size();
    
    // Preprocessing JPEG -> RGB
    ESP_LOGI(TAG, "Preprocessing JPEG -> RGB");
    ESP_LOGI(TAG, "Memoria libera prima della decodifica: %lu bytes", esp_get_free_heap_size());
    
    // Forza allocazione nella PSRAM se disponibile
    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "PSRAM libera: %zu bytes", free_psram);
    
    // Prepara struttura JPEG per ESP-DL
    dl::image::jpeg_img_t jpeg_img = {
        .data = (void*)jpeg_data,
        .data_len = jpeg_size
    };
    
    // Decodifica JPEG usando ESP-DL (ora con PSRAM abilitata)
    auto img = sw_decode_jpeg(jpeg_img, dl::image::DL_IMAGE_PIX_TYPE_RGB888);
    if (!img.data) {
        ESP_LOGE(TAG, "Errore decodifica JPEG");
        return false;
    }
    
    // Face detection usando COCODetect (rileva persone)
    bool face_detected = false;
    float max_confidence = 0.0f;
    
    if (img.data && img.width > 0 && img.height > 0) {
        ESP_LOGI(TAG, "Analisi immagine con COCODetect: %dx%d", img.width, img.height);
        
        COCODetect *detect = new COCODetect();
        auto &detect_results = detect->run(img);
        
        // Cerca persone (category 0 in COCO è "person")
        for (const auto &res : detect_results) {
            if (res.category == 0) { // 0 = person in COCO dataset
                ESP_LOGI(TAG, "Persona rilevata: score=%.3f, box=[%d,%d,%d,%d]", 
                         res.score, res.box[0], res.box[1], res.box[2], res.box[3]);
                
                if (res.score > max_confidence) {
                    max_confidence = res.score;
                    face_detected = true;
                }
            }
        }
        
        if (!face_detected) {
            ESP_LOGI(TAG, "Nessuna persona rilevata");
        }
        
        delete detect;
    }
    heap_caps_free(img.data);
    
    uint32_t end_time = esp_timer_get_time() / 1000;
    uint32_t free_heap_after = esp_get_free_heap_size();
    
    // Popola il risultato
    result->face_detected = face_detected;
    result->confidence = max_confidence;
    result->inference_time_ms = end_time - start_time;
    result->memory_used_kb = (free_heap_before - free_heap_after) / 1024;
    
    // Aggiorna statistiche
    g_stats.total_inferences++;
    g_stats.avg_inference_time_ms = 
        (g_stats.avg_inference_time_ms * (g_stats.total_inferences - 1) + result->inference_time_ms) / 
        g_stats.total_inferences;
    
    if (result->memory_used_kb > g_stats.max_memory_used_kb) {
        g_stats.max_memory_used_kb = result->memory_used_kb;
    }
    
    ESP_LOGI(TAG, "Inferenza completata: %s (conf: %.2f, tempo: %dms, mem: %dKB)", 
             result->face_detected ? "FACCIA" : "NO_FACCIA",
             result->confidence,
             result->inference_time_ms,
             result->memory_used_kb);
    
    return true;
}

void inference_get_stats(inference_stats_t* stats) {
    if (stats) {
        memcpy(stats, &g_stats, sizeof(inference_stats_t));
    }
}

void inference_deinit(void) {
    if (!g_initialized) {
        return;
    }
    
    ESP_LOGI(TAG, "Deinizializzazione sistema di inferenza...");
    
    // COCODetect gestisce internamente i buffer, non serve deallocare nulla
    
    g_initialized = false;
    ESP_LOGI(TAG, "Sistema di inferenza deinizializzato");
} 