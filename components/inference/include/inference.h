#ifndef INFERENCE_H
#define INFERENCE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

// Struttura per i risultati dell'inferenza
typedef struct {
    bool face_detected;
    float confidence;
    uint32_t preprocessing_time_ms; //tempo di esecuzione preprocessing
    uint32_t processing_time_ms; //tempo di esecuzione singola inferenza
    uint32_t postprocessing_time_ms; //tempo di esecuzione postprocessing
    uint32_t full_inference_time_ms; //tempo di esecuzione totale inferenza (preprocessing + inferenza + postprocessing)
    uint32_t memory_used_kb;
    uint32_t bounding_boxes[4];
    uint32_t keypoints[10]; // Keypoints del viso [x1,y1,x2,y2,x3,y3,x4,y4,x5,y5]
    uint32_t num_keypoints; // Numero di keypoints rilevati
    uint32_t category; // Categoria del viso
    uint32_t num_faces;
} inference_result_t;

// Struttura per le statistiche del sistema
typedef struct {
    uint32_t total_inferences;
    uint32_t avg_inference_time_ms;
    uint32_t max_memory_used_kb;
} inference_stats_t;

// Struttura per il sistema di inferenza (classe C-style)
typedef struct {
    bool initialized;
    bool face_detector_initialized;
    inference_stats_t stats;
    void* face_detector; // Puntatore opaco al detector
} inference_t;

/**
 * @brief Inizializza il sistema di inferenza generale
 * @param inf Puntatore alla struttura inference
 * @return true se l'inizializzazione è riuscita, false altrimenti
 */
bool inference_init(inference_t *inf);

/**
 * @brief Inizializza il detector per face detection
 * @param inf Puntatore alla struttura inference
 * @return true se l'inizializzazione è riuscita, false altrimenti
 */
bool inference_face_detector_init(inference_t *inf);

/**
 * @brief Elabora un'immagine JPEG e esegue l'inferenza di face detection
 * @param inf Puntatore alla struttura inference
 * @param jpeg_data Puntatore ai dati JPEG
 * @param jpeg_size Dimensione dei dati JPEG
 * @param result Puntatore alla struttura risultato
 * @return true se l'inferenza è riuscita, false altrimenti
 */
bool inference_face_detection(inference_t *inf, const uint8_t* jpeg_data, size_t jpeg_size, inference_result_t* result);

/**
 * @brief Elabora un'immagine JPEG e esegue l'inferenza (versione legacy)
 * @param jpeg_data Puntatore ai dati JPEG
 * @param jpeg_size Dimensione dei dati JPEG
 * @param result Puntatore alla struttura risultato
 * @return true se l'inferenza è riuscita, false altrimenti
 */
bool inference_process_image(const uint8_t* jpeg_data, size_t jpeg_size, inference_result_t* result);

/**
 * @brief Ottiene le statistiche del sistema di inferenza
 * @param inf Puntatore alla struttura inference
 * @param stats Puntatore alla struttura statistiche
 */
void inference_get_stats(inference_t *inf, inference_stats_t* stats);

/**
 * @brief Ottiene le statistiche del sistema di inferenza (versione legacy)
 * @param stats Puntatore alla struttura statistiche
 */
void inference_get_stats_legacy(inference_stats_t* stats);

/**
 * @brief Deinizializza il detector per face detection
 * @param inf Puntatore alla struttura inference
 */
void inference_face_detector_deinit(inference_t *inf);

/**
 * @brief Deinizializza il sistema di inferenza
 * @param inf Puntatore alla struttura inference
 */
void inference_deinit(inference_t *inf);

/**
 * @brief Deinizializza il sistema di inferenza (versione legacy)
 */
void inference_deinit_legacy(void);

/**
 * @brief Inizializza il sistema di inferenza (versione legacy)
 * @return true se l'inizializzazione è riuscita, false altrimenti
 */
bool inference_init_legacy(void);



#ifdef __cplusplus
}
#endif

#endif // INFERENCE_H 