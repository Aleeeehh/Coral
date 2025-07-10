#ifndef CAMERA_H
#define CAMERA_H

#include "esp_err.h"
#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Struttura per le informazioni di risoluzione
typedef struct {
    int index;
    framesize_t framesize;
    const int width;
    const int height;
} camera_resolution_info_t;

// Classe Camera
typedef struct {
    // Buffer e stato foto
    uint8_t *last_photo_buffer;
    size_t last_photo_size;
    uint32_t last_photo_timestamp;
    
    // Mutex per thread safety
    SemaphoreHandle_t camera_mutex;
    
    // Configurazione risoluzione
    framesize_t current_framesize;
    int current_resolution_index;
    
    // Configurazione camera
    camera_config_t camera_config;
    
    // Stato inizializzazione
    bool initialized;
} camera_t;

/**
 * @brief Inizializza la fotocamera
 * @param camera Puntatore alla struttura camera
 * @return ESP_OK se successo, errore altrimenti
 */
esp_err_t camera_init(camera_t *camera);

/**
 * @brief Deinizializza la fotocamera
 * @param camera Puntatore alla struttura camera
 * @return ESP_OK se successo, errore altrimenti
 */
esp_err_t camera_deinit(camera_t *camera);

/**
 * @brief Scatta una foto e la salva in memoria
 * @param camera Puntatore alla struttura camera
 * @return ESP_OK se successo, errore altrimenti
 */
esp_err_t camera_capture_photo(camera_t *camera);

/**
 * @brief Ottiene l'ultima foto scattata
 * @param camera Puntatore alla struttura camera
 * @param buffer Puntatore al buffer della foto
 * @param size Dimensione del buffer
 * @return ESP_OK se successo, errore altrimenti
 */
esp_err_t camera_get_last_photo(camera_t *camera, uint8_t **buffer, size_t *size);

/**
 * @brief Ottiene le dimensioni della risoluzione corrente
 * @param camera Puntatore alla struttura camera
 * @param width Larghezza
 * @param height Altezza
 */
void camera_get_current_resolution(camera_t *camera, int *width, int *height);

/**
 * @brief Cambia la risoluzione della fotocamera
 * @param camera Puntatore alla struttura camera
 * @param direction 1 per incrementare, 0 per decrementare
 * @return ESP_OK se successo, errore altrimenti
 */
esp_err_t camera_change_resolution(camera_t *camera, int direction);

/**
 * @brief Ottiene il numero totale di risoluzioni disponibili
 * @return Numero di risoluzioni
 */
int camera_get_resolution_count(void);

/**
 * @brief Ottiene le informazioni di una risoluzione specifica
 * @param index Indice della risoluzione
 * @return Puntatore alle informazioni della risoluzione, NULL se non valida
 */
const camera_resolution_info_t* camera_get_resolution_info(int index);

#ifdef __cplusplus
}
#endif

#endif // CAMERA_H 