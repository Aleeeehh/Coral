#include "inference.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "dl_image.hpp"
#include "human_face_detect.hpp"
#include "monitor.h"
#include <string.h>

static const char* TAG = "INFERENCE";

// Variabile globale per il sistema di inferenza (singleton per compatibilità)
static inference_t g_inference;

// Funzione helper per ottenere l'istanza globale (per compatibilità con codice esistente)
static inference_t* get_inference_instance(void) {
    return &g_inference;
}

bool inference_init(inference_t *inf) {
    if (!inf) {
        ESP_LOGE(TAG, "Parametro inference non valido");
        return false;
    }
    
    ESP_LOGI(TAG, "Inizializzazione sistema di inferenza generale...");
    
    if (inf->initialized) {
        ESP_LOGW(TAG, "Sistema già inizializzato");
        return true;
    }
    
    // Reset struttura
    memset(inf, 0, sizeof(inference_t));
    
    inf->initialized = true;
    ESP_LOGI(TAG, "Sistema di inferenza generale inizializzato con successo");
    return true;
}

bool inference_face_detector_init(inference_t *inf) {
    if (!inf || !inf->initialized) {
        ESP_LOGE(TAG, "Sistema di inferenza non inizializzato");
        return false;
    }
    
    if (inf->face_detector_initialized) {
        ESP_LOGW(TAG, "Face detector già inizializzato");
        return true;
    }
    
    ESP_LOGI(TAG, "Inizializzazione face detector HumanFaceDetect...");

    printf("Snapshot della PSRAM prima di inizializzare il face detector\n");
    monitor_log_ram_usage("INFERENCE_FACE_DETECTOR_START");
    monitor_print_ram_stats();
    //monitor_memory_region_details();
    
    // Crea il detector per face detection
    inf->face_detector = new HumanFaceDetect(); //MSRMNP_S8_V1
    if (!inf->face_detector) {
        ESP_LOGE(TAG, "Errore creazione HumanFaceDetect");
        return false;
    }

    printf("Snapshot della PSRAM dopo aver inizializzato il face detector\n");
    monitor_log_ram_usage("INFERENCE_FACE_DETECTOR_START");
    monitor_print_ram_stats();
    //monitor_memory_region_details();
    
    inf->face_detector_initialized = true;
    ESP_LOGI(TAG, "Face detector HumanFaceDetect inizializzato con successo");
    return true;
}

bool inference_face_detection(inference_t *inf, const uint8_t* jpeg_data, size_t jpeg_size, inference_result_t* result) {
    if (!inf || !inf->initialized || !inf->face_detector_initialized || !jpeg_data || !result || !inf->face_detector) {
        ESP_LOGE(TAG, "Parametri non validi o sistema non inizializzato");
        return false;
    }
    
    ESP_LOGI(TAG, "Elaborazione immagine JPEG (%zu bytes) con HumanFaceDetect", jpeg_size);
    
    // Avvia monitoraggio dell'inferenza
    monitor_inference_start();
    monitor_log_ram_usage("INFERENCE_START");
    
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
    const float CONFIDENCE_THRESHOLD = 0.5f; // Soglia del 50%, si può cambiare
    
    if (img.data && img.width > 0 && img.height > 0) {
        HumanFaceDetect* detector = static_cast<HumanFaceDetect*>(inf->face_detector);
        auto &detect_results = detector->run(img); //esegui l'inferenza
        result->num_faces = detect_results.size();

        // Controlla se sono state rilevate facce
        for (const auto &res : detect_results) {
            ESP_LOGI(TAG, "Faccia rilevata: score=%.3f, box=[%d,%d,%d,%d]", 
                     res.score, res.box[0], res.box[1], res.box[2], res.box[3]);
            
            //popola le bounding boxes
            result->bounding_boxes[0] = res.box[0];
            result->bounding_boxes[1] = res.box[1];
            result->bounding_boxes[2] = res.box[2];
            result->bounding_boxes[3] = res.box[3];

            // Considera solo facce con confidenza sopra la soglia
            if (res.score >= CONFIDENCE_THRESHOLD) {
                if (res.score > max_confidence) {
                    max_confidence = res.score;
                }
                face_detected = true;
            } else {
                ESP_LOGI(TAG, "Faccia scartata: confidenza %.3f < soglia %.3f", 
                         res.score, CONFIDENCE_THRESHOLD);
            }
        }
        
        if (!face_detected) {
            ESP_LOGI(TAG, "Nessuna faccia rilevata con confidenza >= %.1f%%", 
                     CONFIDENCE_THRESHOLD * 100);
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
    inf->stats.total_inferences++;
    inf->stats.avg_inference_time_ms = 
        (inf->stats.avg_inference_time_ms * (inf->stats.total_inferences - 1) + result->inference_time_ms) / 
        inf->stats.total_inferences;
    
    if (result->memory_used_kb > inf->stats.max_memory_used_kb) {
        inf->stats.max_memory_used_kb = result->memory_used_kb;
    }
    
    // Termina monitoraggio dell'inferenza
    monitor_inference_end();
    monitor_log_ram_usage("INFERENCE_END");
    
    ESP_LOGI(TAG, "Inferenza completata: %s (conf: %.2f, tempo: %dms, mem: %dKB)", 
             result->face_detected ? "FACCIA" : "NO_FACCIA",
             result->confidence,
             result->inference_time_ms,
             result->memory_used_kb,
             result->bounding_boxes[0],
             result->bounding_boxes[1],
             result->bounding_boxes[2],
             result->bounding_boxes[3],
             result->num_faces);
    
    return true;
}

void inference_get_stats(inference_t *inf, inference_stats_t* result_stats) {
    if (result_stats && inf) {
        memcpy(result_stats, &inf->stats, sizeof(inference_stats_t));
    }
}

void inference_face_detector_deinit(inference_t *inf) {
    if (!inf || !inf->face_detector_initialized) {
        return;
    }
    
    ESP_LOGI(TAG, "Deinizializzazione face detector HumanFaceDetect...");
    
    if (inf->face_detector) {
        HumanFaceDetect* detector = static_cast<HumanFaceDetect*>(inf->face_detector);
        delete detector;
        inf->face_detector = nullptr;
    }
    
    inf->face_detector_initialized = false;
    ESP_LOGI(TAG, "Face detector HumanFaceDetect deinizializzato");
}

void inference_deinit(inference_t *inf) {
    if (!inf || !inf->initialized) {
        return;
    }
    
    ESP_LOGI(TAG, "Deinizializzazione sistema di inferenza...");
    
    // Deinizializza prima il face detector
    inference_face_detector_deinit(inf);
    
    inf->initialized = false;
    ESP_LOGI(TAG, "Sistema di inferenza deinizializzato");
}

// ===== FUNZIONI LEGACY PER COMPATIBILITÀ =====

bool inference_init_legacy(void) {
    inference_t *inf = get_inference_instance();
    return inference_init(inf) && inference_face_detector_init(inf);
}

bool inference_process_image(const uint8_t* jpeg_data, size_t jpeg_size, inference_result_t* result) {
    inference_t *inf = get_inference_instance();
    return inference_face_detection(inf, jpeg_data, jpeg_size, result);
}

void inference_get_stats_legacy(inference_stats_t* result_stats) {
    inference_t *inf = get_inference_instance();
    inference_get_stats(inf, result_stats);
}

void inference_deinit_legacy(void) {
    inference_t *inf = get_inference_instance();
    inference_deinit(inf);
} 