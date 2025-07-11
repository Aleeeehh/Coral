#include "camera.h"
#include "inference.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "inference.h"
#include <string.h>

static const char *TAG = "CAMERA";

// Queue per comunicazione con AI task
static QueueHandle_t ai_task_queue = NULL;

// Mappa delle risoluzioni disponibili
static const camera_resolution_info_t resolution_map[] = {
    //{0, FRAMESIZE_96X96, 96, 96},    // 96x96 (causa problemi di stabilità)
    {0, FRAMESIZE_QQVGA, 160, 120},    // 160x120
    {1, FRAMESIZE_128X128, 128, 128},    // 128x128
    {2, FRAMESIZE_QCIF, 176, 144},     // 176x144
    {3, FRAMESIZE_HQVGA, 240, 176},    // 240x176
    {4, FRAMESIZE_240X240, 240, 240},  // 240x240
    {5, FRAMESIZE_QVGA, 320, 240},     // 320x240
    {6, FRAMESIZE_320X320, 320, 320},  // 320x320
    {7, FRAMESIZE_CIF, 400, 296},      // 400x296
    {8, FRAMESIZE_HVGA, 480, 320},     // 480x320
    {9, FRAMESIZE_VGA, 640, 480},      // 640x480
    {10, FRAMESIZE_SVGA, 800, 600},     // 800x600
    {11, FRAMESIZE_XGA, 1024, 768},      // 1024x768
    {12, FRAMESIZE_HD, 1280, 720},       // 1280x720
    {13, FRAMESIZE_SXGA, 1280, 1024},     // 1280x1024
    {14, FRAMESIZE_UXGA, 1600, 1200},     // 1600x1200
    // 3MP Sensors
    {15, FRAMESIZE_FHD, 1920, 1080},      // 1920x1080
    //{16, FRAMESIZE_QXGA, 2048, 1536},     // 2048x1536
    //FRAMESIZE_P_3MP,  // 864x1536, registrazione video 3MP per dispositivi mobili
};

// Configurazione camera esp32-s3 ai camera
static const camera_config_t default_camera_config = {
  .pin_pwdn      = -1,
  .pin_reset     = -1,
  .pin_xclk      = 5,
  .pin_sccb_sda  = 8,
  .pin_sccb_scl  = 9,
  .pin_d7        = 4,
  .pin_d6        = 6,
  .pin_d5        = 7,
  .pin_d4        = 14,
  .pin_d3        = 17,
  .pin_d2        = 21,
  .pin_d1        = 18,
  .pin_d0        = 16,
  .pin_vsync     = 1,
  .pin_href      = 2,
  .pin_pclk      = 15,
  .xclk_freq_hz  = 20000000,
  .ledc_timer    = LEDC_TIMER_0,
  .ledc_channel  = LEDC_CHANNEL_0,
  .pixel_format  = PIXFORMAT_JPEG,
  .frame_size    = FRAMESIZE_QVGA, //Dimensione del frame della fotocamera
  .jpeg_quality  = 10, // più è alto, e più l'immagine è compressa
  .fb_count      = 1,
  .fb_location   = CAMERA_FB_IN_PSRAM, //Usa PSRAM per i buffer della fotocamera
  .grab_mode     = CAMERA_GRAB_LATEST, // Cambiato per gestire meglio risoluzioni basse
  .sccb_i2c_port = 0
};

esp_err_t camera_init(camera_t *camera)
{
    if (!camera) {
        ESP_LOGE(TAG, "Puntatore camera NULL");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Inizializzazione fotocamera ESP32CAM...");

    // Inizializza struttura camera
    memset(camera, 0, sizeof(camera_t));
    
    // Copia configurazione di default
    memcpy(&camera->camera_config, &default_camera_config, sizeof(camera_config_t));
    
    // Imposta risoluzione di default
    camera->current_resolution_index = 5; // QVGA
    camera->current_framesize = resolution_map[camera->current_resolution_index].framesize;
    camera->camera_config.frame_size = camera->current_framesize;

    // Crea un semaforo mutex (inizializzato ad 1) per l'accesso thread-safe sulla fotocamera
    camera->camera_mutex = xSemaphoreCreateMutex();
    if (camera->camera_mutex == NULL)
    {
        ESP_LOGE(TAG, "Errore creazione mutex fotocamera");
        return ESP_FAIL;
    }

    // Inizializza la fotocamera
    esp_err_t ret = esp_camera_init(&camera->camera_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x: %s", ret, esp_err_to_name(ret));
        return ret;
    }
    else
    {
        ESP_LOGI(TAG, "Camera inizializzata con successo");
    }

    camera->initialized = true;
    return ESP_OK;
}

esp_err_t camera_deinit(camera_t *camera)
{
    if (!camera) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Deinizializzazione fotocamera...");

    // Libera memoria foto
    if (camera->last_photo_buffer) {
        free(camera->last_photo_buffer);
        camera->last_photo_buffer = NULL;
        camera->last_photo_size = 0;
    }

    // Deinizializza camera
    esp_err_t ret = esp_camera_deinit();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Errore deinit camera: %s", esp_err_to_name(ret));
    }

    // Elimina mutex
    if (camera->camera_mutex) {
        vSemaphoreDelete(camera->camera_mutex);
        camera->camera_mutex = NULL;
    }

    camera->initialized = false;
    return ret;
}

esp_err_t camera_capture_photo(camera_t *camera)
{
    if (!camera || !camera->initialized) {
        ESP_LOGE(TAG, "Camera non inizializzata");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Acquisizione foto...");

    //attende 5 secondi e prendere il mutex, se non riesce dopo 5sec ritorna errore
    if (xSemaphoreTake(camera->camera_mutex, pdMS_TO_TICKS(5000)) != pdTRUE)
    {
        ESP_LOGE(TAG, " Timeout acquisizione mutex fotocamera");
        return ESP_ERR_TIMEOUT;
    }

    // Libera memoria foto precedente
    if (camera->last_photo_buffer != NULL)
    {
        ESP_LOGI(TAG, "Liberazione memoria foto precedente");
        free(camera->last_photo_buffer);
        camera->last_photo_buffer = NULL;
        camera->last_photo_size = 0;
    }

    // Scarta il primo frame (potrebbe essere vecchio)
    ESP_LOGI(TAG, "Scarto primo frame (potrebbe essere vecchio)...");
    camera_fb_t *fb_old = esp_camera_fb_get(); //acquisisce il frame 
    if (fb_old) {
        ESP_LOGI(TAG, "Frame vecchio scartato: %d bytes", fb_old->len);
        esp_camera_fb_return(fb_old); //riempie il buffer della fotocamera col frame 
    }

    // Piccolo delay per permettere alla fotocamera di acquisire un nuovo frame
    vTaskDelay(pdMS_TO_TICKS(100));

    // Acquisisci il frame fresco
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
        ESP_LOGE(TAG, "Errore acquisizione frame fotocamera");
        xSemaphoreGive(camera->camera_mutex);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Frame fresco acquisito: %d bytes", fb->len);

    // Alloca memoria per salvare la foto
    camera->last_photo_buffer = (uint8_t *)malloc(fb->len);
    if (camera->last_photo_buffer == NULL)
    {
        ESP_LOGE(TAG, "Errore allocazione memoria per foto");
        esp_camera_fb_return(fb);
        xSemaphoreGive(camera->camera_mutex);
        return ESP_ERR_NO_MEM;
    }

    // Copia i dati
    //sposta fb->len byte dal buffer fb->buf (memoria temporanea) al buffer last_photo_buffer (memoria permanente)
    memcpy(camera->last_photo_buffer, fb->buf, fb->len);
    camera->last_photo_size = fb->len;
    camera->last_photo_timestamp = esp_timer_get_time() / 1000000;

    // Restituisci il frame buffer
    esp_camera_fb_return(fb);

    ESP_LOGI(TAG, "Foto salvata: %d bytes", camera->last_photo_size);

    xSemaphoreGive(camera->camera_mutex); // rilascia il mutex, permettendo a altri task di accedere alla fotocamera
    return ESP_OK;
}

esp_err_t camera_get_last_photo(camera_t *camera, uint8_t **buffer, size_t *size)
{
    if (!camera || !buffer || !size) {
        return ESP_ERR_INVALID_ARG;
    }

    if (camera->last_photo_buffer == NULL || camera->last_photo_size == 0)
    {
        return ESP_ERR_NOT_FOUND;
    }

    *buffer = camera->last_photo_buffer;
    *size = camera->last_photo_size;
    return ESP_OK;
}

void camera_get_current_resolution(camera_t *camera, int *width, int *height)
{
    if (!camera || !width || !height) {
        return;
    }

    if (camera->current_resolution_index >= 0 && 
        camera->current_resolution_index < sizeof(resolution_map) / sizeof(resolution_map[0])) {
        *width = resolution_map[camera->current_resolution_index].width;
        *height = resolution_map[camera->current_resolution_index].height;
    } else {
        *width = 0;
        *height = 0;
    }
}

esp_err_t camera_change_resolution(camera_t *camera, int direction)
{
    if (!camera || !camera->initialized) {
        ESP_LOGE(TAG, "Camera non inizializzata");
        return ESP_ERR_INVALID_STATE;
    }

    //attende 5 secondi e prendere il mutex, se non riesce dopo 5sec ritorna errore
    if (xSemaphoreTake(camera->camera_mutex, pdMS_TO_TICKS(5000)) != pdTRUE)
    {
        ESP_LOGE(TAG, " Timeout acquisizione mutex fotocamera");
        return ESP_ERR_TIMEOUT;
    }
        
    // Cambia l'indice in base alla direzione
    if (direction == 0) {
        // Decrementa
        camera->current_resolution_index--;
        if (camera->current_resolution_index < 0) {
            camera->current_resolution_index = sizeof(resolution_map) / sizeof(resolution_map[0]) - 1;
        }
    } else {
        // Incrementa
        camera->current_resolution_index++;
        if (camera->current_resolution_index >= sizeof(resolution_map) / sizeof(resolution_map[0])) {
            camera->current_resolution_index = 0;
        }
    }
    
    // Imposta la nuova risoluzione
    framesize_t new_framesize = resolution_map[camera->current_resolution_index].framesize;
    camera->current_framesize = new_framesize; 
    camera->camera_config.frame_size = new_framesize;
    
    ESP_LOGI(TAG, "Direction: %d, Current index: %d", direction, camera->current_resolution_index);

    vTaskDelay(pdMS_TO_TICKS(100));

    //deinizializza la camera
    esp_err_t ret1 = esp_camera_deinit();           // stop camera
    if (ret1 != ESP_OK) {
        ESP_LOGE(TAG, "Errore deinit camera: %s", esp_err_to_name(ret1));
    }
    else
    {
        ESP_LOGI(TAG, "Camera deinizializzata con successo");
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    //riavvia la camera con la nuova configurazione
    esp_err_t ret2 = esp_camera_init(&camera->camera_config); // riavvio con nuova configurazione
    if (ret2 != ESP_OK) {
        ESP_LOGE(TAG, "Errore riavvio camera: %s", esp_err_to_name(ret2));
    }
    else
    {
        ESP_LOGI(TAG, "Camera riavviata con successo");
    }

    xSemaphoreGive(camera->camera_mutex);

    // Ottieni dimensioni della risoluzione corrente
    int width, height;
    camera_get_current_resolution(camera, &width, &height);
    printf("===========================\n");
    printf("Risoluzione impostata: %dx%d\n", width, height);
    printf("===========================\n");


    return ESP_OK;
}

int camera_get_resolution_count(void)
{
    return sizeof(resolution_map) / sizeof(resolution_map[0]);
}

const camera_resolution_info_t* camera_get_resolution_info(int index)
{
    if (index >= 0 && index < sizeof(resolution_map) / sizeof(resolution_map[0])) {
        return &resolution_map[index];
    }
    return NULL;
}

esp_err_t camera_capture_and_inference(camera_t *camera, inference_result_t *result)
{
    if (!camera || !camera->initialized) {
        ESP_LOGE(TAG, "Camera non inizializzata");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Avvio scatto foto e invio alla AI task...");
    
    // Scatta una nuova foto
    esp_err_t ret = camera_capture_photo(camera);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Errore durante lo scatto della foto");
        return ret;
    }
    
    // Ottieni i dati della foto
    uint8_t *photo_buffer;
    size_t photo_size;
    ret = camera_get_last_photo(camera, &photo_buffer, &photo_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Nessuna foto disponibile per l'inferenza");
        return ret;
    }
    
    // Alloca memoria per copiare il frame (la AI task lo libererà)
    uint8_t *frame_copy = (uint8_t *)malloc(photo_size);
    if (!frame_copy) {
        ESP_LOGE(TAG, "Errore allocazione memoria per copia frame");
        return ESP_ERR_NO_MEM;
    }
    
    // Copia il frame
    memcpy(frame_copy, photo_buffer, photo_size);
    
    // Prepara messaggio per AI task
    ai_task_message_t message = {
        .image_buffer = frame_copy,
        .image_size = photo_size,
        .timestamp = (uint32_t)(esp_timer_get_time() / 1000000)
    };
    
    // Invia il messaggio alla AI task
    if (xQueueSend(ai_task_queue, &message, pdMS_TO_TICKS(5000)) != pdTRUE) {
        ESP_LOGE(TAG, "Timeout invio messaggio alla AI task");
        free(frame_copy);
        return ESP_ERR_TIMEOUT;
    }
    
    ESP_LOGI(TAG, "Frame inviato alla AI task per inferenza");
    
    // Se il parametro result non è NULL, per ora restituiamo un risultato vuoto
    // (in futuro potremmo implementare una queue di risposta)
    if (result != NULL) {
        memset(result, 0, sizeof(inference_result_t));
    }

    return ESP_OK;
} 

esp_err_t camera_init_ai_queue(void)
{
    ESP_LOGI(TAG, "Inizializzazione queue per AI task...");
    
    // Crea la queue per i messaggi (max 5 messaggi in coda)
    ai_task_queue = xQueueCreate(5, sizeof(ai_task_message_t));
    if (ai_task_queue == NULL) {
        ESP_LOGE(TAG, "Errore creazione queue per AI task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Queue per AI task creata con successo");
    return ESP_OK;
}

QueueHandle_t camera_get_ai_queue(void)
{
    return ai_task_queue;
}