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
uint32_t start_time_full_inference;
uint32_t end_time_full_inference;
uint32_t start_time_processing;
uint32_t end_time_processing;
uint32_t start_time_preprocessing;
uint32_t end_time_preprocessing;
uint32_t start_time_postprocessing;
uint32_t end_time_postprocessing;

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

    // Stampa task corrente
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
    const char* task_name = pcTaskGetName(current_task);
    printf("Task corrente: %s\n", task_name);
    printf("Task corrente: %s\n", task_name);
    printf("Task corrente: %s\n", task_name);
    printf("Task corrente: %s\n", task_name);
    printf("Task corrente: %s\n", task_name);
    printf("Task corrente: %s\n", task_name);
    printf("Task corrente: %s\n", task_name);
    printf("Task corrente: %s\n", task_name);

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
    monitor_log_ram_usage("INFERENCE_FACE_DETECTOR_END");
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

    start_time_full_inference = esp_timer_get_time() / 1000;  //inizia a contare tempo inferenza totale
    
    //Preprocessing
    start_time_preprocessing = esp_timer_get_time() / 1000;  //inizia a contare tempo preprocessing
    
    // Prepara struttura JPEG interpretabile dal decoder JPEG di ESP-DL
    dl::image::jpeg_img_t jpeg_img = {
        .data = (void*)jpeg_data,
        .data_len = jpeg_size
    };
    
    // Decodifica JPEG grezzo della fotocamera in un formato RGB888 comprensibile con il modello
    auto img = sw_decode_jpeg(jpeg_img, dl::image::DL_IMAGE_PIX_TYPE_RGB888);
    if (!img.data) {
        ESP_LOGE(TAG, "Errore decodifica JPEG");
        return false;
    }
    end_time_preprocessing = esp_timer_get_time() / 1000; //smetti di contare tempo preprocessing

    bool face_detected = false;
    float max_confidence = 0.0f;
    const float CONFIDENCE_THRESHOLD = 0.5f; // Soglia del 50%, si può cambiare

    
    if (img.data && img.width > 0 && img.height > 0) {
        //Processing
        HumanFaceDetect* detector = static_cast<HumanFaceDetect*>(inf->face_detector);
        start_time_processing = esp_timer_get_time() / 1000;  //inizia a contare tempo inferenza 
        auto &detect_results = detector->run(img); //esegui l'inferenza
        end_time_processing = esp_timer_get_time() / 1000; //smetti di contare tempo inferenza

        // Stampa risultati grezzi del modello per analisi
        printf("=== RISULTATI GREZZI DEL MODELLO ===\n");
        printf("Numero detections grezze: %zu\n", detect_results.size());
        size_t i = 0;
        for (const auto &res : detect_results) {
            printf("Detection %zu: category=%d, score=%.4f, box=[%d,%d,%d,%d]\n", 
                   i, res.category, res.score, res.box[0], res.box[1], res.box[2], res.box[3]);
            
            // Stampa keypoints se presenti
            if (!res.keypoint.empty()) {
                printf("  Keypoints (%zu): ", res.keypoint.size());
                for (size_t k = 0; k < res.keypoint.size(); k++) {
                    printf("%d", res.keypoint[k]);
                    if (k < res.keypoint.size() - 1) printf(",");
                }
                printf("\n");
            }
            
            // Stampa area della bounding box
            printf("  Box area: %d pixels\n", res.box_area());
            
            i++;
        }
        printf("=====================================\n");

        //Postprocessing
        start_time_postprocessing = esp_timer_get_time() / 1000;  //inizia a contare tempo postprocessing
        result->num_faces = detect_results.size();

        // Controlla se sono state rilevate facce
        // Filtra e interpreta i risultati grezzi del modello
        for (const auto &res : detect_results) {
            ESP_LOGI(TAG, "Faccia rilevata: score=%.3f, box=[%d,%d,%d,%d]", 
                     res.score, res.box[0], res.box[1], res.box[2], res.box[3]);
            
            //popola le bounding boxes
            result->bounding_boxes[0] = res.box[0];
            result->bounding_boxes[1] = res.box[1];
            result->bounding_boxes[2] = res.box[2];
            result->bounding_boxes[3] = res.box[3];

            //popola le keypoints con ciclo
            result->num_keypoints = res.keypoint.size();
            for (size_t k = 0; k < res.keypoint.size(); k++) {
                result->keypoints[k] = res.keypoint[k];
            }

            //popola la categoria
            result->category = res.category;
            
            //TODO: ha senso?
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
    end_time_postprocessing = esp_timer_get_time() / 1000; //smetti di contare tempo postprocessing
    end_time_full_inference = esp_timer_get_time() / 1000; //smetti di contare tempo inferenza totale


    // Libera memoria immagine
    heap_caps_free(img.data);
        
    // Popola il risultato
    result->face_detected = face_detected;
    result->confidence = max_confidence;
    result->processing_time_ms = end_time_processing - start_time_processing;
    result->preprocessing_time_ms = end_time_preprocessing - start_time_preprocessing;
    result->postprocessing_time_ms = end_time_postprocessing - start_time_postprocessing;
    result->full_inference_time_ms = end_time_full_inference - start_time_full_inference;
  

    
    // Aggiorna statistiche
    inf->stats.total_inferences++;
    inf->stats.avg_inference_time_ms = 
        (inf->stats.avg_inference_time_ms * (inf->stats.total_inferences - 1) + result->full_inference_time_ms) / 
        inf->stats.total_inferences;
    
    if (result->memory_used_kb > inf->stats.max_memory_used_kb) {
        inf->stats.max_memory_used_kb = result->memory_used_kb;
    }

    
    ESP_LOGI(TAG, "Inferenza completata: %s (conf: %.2f, tempo: %dms, mem: %dKB)", 
             result->face_detected ? "FACCIA" : "NO_FACCIA",
             result->confidence,
             result->processing_time_ms,
             result->preprocessing_time_ms,
             result->postprocessing_time_ms,
             result->full_inference_time_ms,
             result->memory_used_kb,
             result->bounding_boxes[0],
             result->bounding_boxes[1],
             result->bounding_boxes[2],
             result->bounding_boxes[3],
             result->category,
             result->num_faces);
    
    // Stampa i risultati per CLI
    printf("=== RISULTATI INFERENZA ===\n");
    printf("Volto rilevato: %s\n", result->face_detected ? "SI" : "NO");
    printf("Confidenza: %.3f\n", result->confidence);
    printf("Tempo preprocessing: %lu ms\n", result->preprocessing_time_ms);
    printf("Tempo processing inferenza: %lu ms\n", result->processing_time_ms);
    printf("Tempo postprocessing: %lu ms\n", result->postprocessing_time_ms);
    printf("Tempo inferenza totale: %lu ms\n", result->full_inference_time_ms);
    printf("Numero volti: %lu\n", result->num_faces);
    if (result->face_detected) {
        printf("Bounding box: [%lu, %lu, %lu, %lu]\n", 
                 result->bounding_boxes[0], result->bounding_boxes[1],
                 result->bounding_boxes[2], result->bounding_boxes[3]);
        
        // Stampa keypoints se presenti
        if (result->num_keypoints > 0) {
            printf("Keypoints (%lu): ", result->num_keypoints);
            for (size_t k = 0; k < result->num_keypoints; k++) {
                printf("%lu", result->keypoints[k]);
                if (k < result->num_keypoints - 1) printf(",");
            }
            printf("\n");
        }
    }
    printf("===========================\n");
    
    
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

 