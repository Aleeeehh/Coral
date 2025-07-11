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

    
    if (img.data && img.width > 0 && img.height > 0) {
        //Processing
        HumanFaceDetect* detector = static_cast<HumanFaceDetect*>(inf->face_detector);
        start_time_processing = esp_timer_get_time() / 1000;  //inizia a contare tempo inferenza 
        auto &detect_results = detector->run(img); //esegui l'inferenza
        end_time_processing = esp_timer_get_time() / 1000; //smetti di contare tempo inferenza
        result->num_faces = detect_results.size();
        
        //Postprocessing
        start_time_postprocessing = esp_timer_get_time() / 1000;  //inizia a contare tempo postprocessing

        // Controlla se sono state rilevate facce
        // Filtra e interpreta i risultati grezzi del modello
        int face_index = 0;
        for (const auto &res : detect_results) {
            // Controlla che non superiamo il numero massimo di facce
            if (face_index >= MAX_FACES) {
                ESP_LOGW(TAG, "Numero massimo di facce (%d) raggiunto, saltando detection %d", MAX_FACES, face_index);
                break;
            }
            ESP_LOGI(TAG, "Faccia rilevata: score=%.3f, box=[%d,%d,%d,%d]", 
                     res.score, res.box[0], res.box[1], res.box[2], res.box[3]);
            
            //popola le bounding boxes
            for (int j = 0; j < 4; j++) {
                result->faces[face_index].bounding_boxes[j] = res.box[j];
            }

            //popola le keypoints con ciclo
            result->faces[face_index].num_keypoints = res.keypoint.size();
            for (size_t k = 0; k < res.keypoint.size(); k++) {
                result->faces[face_index].keypoints[k] = res.keypoint[k];
            }

            result->faces[face_index].confidence = res.score;

            //popola la categoria
            result->faces[face_index].category = res.category;
            
            // Accetta tutte le facce rilevate, indipendentemente dalla confidence
            if (res.score > max_confidence) {
                max_confidence = res.score;
            }
            face_detected = true;
            ESP_LOGI(TAG, "Faccia accettata: confidenza %.3f", res.score);
            
            face_index++;
        }
        
        if (!face_detected) {
            ESP_LOGI(TAG, "Nessuna faccia rilevata");
        }
    }


    // Libera memoria immagine
    heap_caps_free(img.data);
    
    // Aggiorna statistiche
    inf->stats.total_inferences++;
    inf->stats.avg_inference_time_ms = 
        (inf->stats.avg_inference_time_ms * (inf->stats.total_inferences - 1) + result->full_inference_time_ms) / 
        inf->stats.total_inferences;

    end_time_postprocessing = esp_timer_get_time() / 1000; //smetti di contare tempo postprocessing
    end_time_full_inference = esp_timer_get_time() / 1000; //smetti di contare tempo inferenza totale

    // Popola il risultato
    result->face_detected = face_detected;
    result->processing_time_ms = end_time_processing - start_time_processing;
    result->preprocessing_time_ms = end_time_preprocessing - start_time_preprocessing;
    result->postprocessing_time_ms = end_time_postprocessing - start_time_postprocessing;
    result->full_inference_time_ms = end_time_full_inference - start_time_full_inference;
  


    // Stampa i risultati per CLI
    printf("=== RISULTATI INFERENZA ===\n");
    printf("Volto rilevato: %s\n", result->face_detected ? "SI" : "NO");
    printf("Tempo preprocessing: %lu ms\n", result->preprocessing_time_ms);
    printf("Tempo processing inferenza: %lu ms\n", result->processing_time_ms);
    printf("Tempo postprocessing: %lu ms\n", result->postprocessing_time_ms);
    printf("Tempo inferenza totale: %lu ms\n", result->full_inference_time_ms);
    printf("Numero volti: %lu\n", result->num_faces);
    //printa bounding box, keypoints e confidenza per ogni faccia
    if (result->face_detected) {
        for (int i = 0; i < result->num_faces; i++) {
            printf("Bounding box: [%lu, %lu, %lu, %lu]\n", 
                 result->faces[i].bounding_boxes[0], result->faces[i].bounding_boxes[1],
                 result->faces[i].bounding_boxes[2], result->faces[i].bounding_boxes[3]);
            printf("Keypoints (%lu): ", result->faces[i].num_keypoints);
            for (size_t k = 0; k < result->faces[i].num_keypoints; k++) {
                printf("%lu, ", result->faces[i].keypoints[k]);
            }
            printf("\n");
            printf("Confidenza: %.3f\n", result->faces[i].confidence);
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

 