#ifndef INFERENCE_H
#define INFERENCE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Struttura per i risultati dell'inferenza
typedef struct {
    bool face_detected;
    float confidence;
    uint32_t inference_time_ms;
    uint32_t memory_used_kb;
} inference_result_t;

// Struttura per le statistiche del sistema
typedef struct {
    uint32_t total_inferences;
    uint32_t avg_inference_time_ms;
    uint32_t max_memory_used_kb;
} inference_stats_t;

/**
 * @brief Inizializza il sistema di inferenza
 * @return true se l'inizializzazione è riuscita, false altrimenti
 */
bool inference_init(void);

/**
 * @brief Elabora un'immagine JPEG e esegue l'inferenza
 * @param jpeg_data Puntatore ai dati JPEG
 * @param jpeg_size Dimensione dei dati JPEG
 * @param result Puntatore alla struttura risultato
 * @return true se l'inferenza è riuscita, false altrimenti
 */
bool inference_process_image(const uint8_t* jpeg_data, size_t jpeg_size, inference_result_t* result);

/**
 * @brief Ottiene le statistiche del sistema di inferenza
 * @param stats Puntatore alla struttura statistiche
 */
void inference_get_stats(inference_stats_t* stats);

/**
 * @brief Deinizializza il sistema di inferenza
 */
void inference_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // INFERENCE_H 