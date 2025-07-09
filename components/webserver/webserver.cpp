#include "webserver.h"
#include "inference.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <fstream>
#include <sstream>

static const char *TAG = "WEBSERVER";

// Dichiarazioni delle funzioni statiche
static esp_err_t camera_init(void);
static esp_err_t camera_capture_photo(void);
static esp_err_t camera_get_last_photo(uint8_t **buffer, size_t *size);
static esp_err_t inference_post_handler(httpd_req_t *req);

// Variabili globali per la fotocamera
static uint8_t *last_photo_buffer = NULL; //buffer di 8 bit per la foto
static size_t last_photo_size = 0; //dimensione della foto, usato size_t per portabilità (valori diversi in base a tipo di esp, in questo caso "32 bit")
static uint32_t last_photo_timestamp = 0; //timestamp della foto
static SemaphoreHandle_t camera_mutex = NULL; //semaforo mutex per la fotocamera
static framesize_t current_framesize = FRAMESIZE_96X96; // default
static int current_resolution_index = 6; //indice della risoluzione corrente

//server
static httpd_handle_t server = NULL;

// Variabile globale per l'IP
static char current_ip[16] = "0.0.0.0"; // IP di default

typedef struct {
    int index;
    framesize_t framesize;
    const int width;
    const int height;
} my_resolution_info_t;

static const my_resolution_info_t resolution_map[] = {
    {0, FRAMESIZE_96X96, 96, 96},    // 96x96
    {1, FRAMESIZE_QQVGA, 160, 120},    // 160x120
    {2, FRAMESIZE_128X128, 128, 128},    // 128x128
    {3, FRAMESIZE_QCIF, 176, 144},     // 176x144
    {4, FRAMESIZE_HQVGA, 240, 176},    // 240x176
    {5, FRAMESIZE_240X240, 240, 240},  // 240x240
    {6, FRAMESIZE_QVGA, 320, 240},     // 320x240
    {7, FRAMESIZE_320X320, 320, 320},  // 320x320
    {8, FRAMESIZE_CIF, 400, 296},      // 400x296
    {9, FRAMESIZE_HVGA, 480, 320},     // 480x320
    {10, FRAMESIZE_VGA, 640, 480},      // 640x480
    {11, FRAMESIZE_SVGA, 800, 600},     // 800x600
    {12, FRAMESIZE_XGA, 1024, 768},      // 1024x768
    {13, FRAMESIZE_HD, 1280, 720},       // 1280x720
    {14, FRAMESIZE_SXGA, 1280, 1024},     // 1280x1024
    {15, FRAMESIZE_UXGA, 1600, 1200},     // 1600x1200
    // 3MP Sensors
    {16, FRAMESIZE_FHD, 1920, 1080},      // 1920x1080
    {17, FRAMESIZE_QXGA, 2048, 1536},     // 2048x1536
    //FRAMESIZE_P_3MP,  // 864x1536, registrazione video 3MP per dispositivi mobili
};

// Configurazione camera esp32-s3 ai camera
static camera_config_t camera_config = {
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
  .grab_mode     = CAMERA_GRAB_WHEN_EMPTY,
  .sccb_i2c_port = 0
};

// Dichiarazioni esterne per il file HTML embedded
extern const uint8_t main_page_html_start[] asm("_binary_main_page_html_start");
extern const uint8_t main_page_html_end[] asm("_binary_main_page_html_end");



//Funzioni helper

// Funzione per impostare l'IP del webserver
void webserver_set_ip(const char* ip_address)
{
    if (ip_address != NULL && strlen(ip_address) < sizeof(current_ip)) {
        strcpy(current_ip, ip_address);
        ESP_LOGI(TAG, "IP del webserver impostato a: %s", current_ip);
    }
}

// ritorna la foto più recentemente scattata
static esp_err_t camera_get_last_photo(uint8_t **buffer, size_t *size)
{
    if (last_photo_buffer == NULL || last_photo_size == 0)
    {
        return ESP_ERR_NOT_FOUND;
    }

    *buffer = last_photo_buffer;
    *size = last_photo_size;
    return ESP_OK;
}

//prendi il mutex per usare la camera, e salva in memoria la foto del buffer di fotocamera
static esp_err_t camera_capture_photo(void)
{
    ESP_LOGI(TAG, "Acquisizione foto...");

    //attende 5 secondi e prendere il mutex, se non riesce dopo 5sec ritorna errore
    if (xSemaphoreTake(camera_mutex, pdMS_TO_TICKS(5000)) != pdTRUE)
    {
        ESP_LOGE(TAG, " Timeout acquisizione mutex fotocamera");
        return ESP_ERR_TIMEOUT;
    }

    // Libera memoria foto precedente
    if (last_photo_buffer != NULL)
    {
        ESP_LOGI(TAG, "Liberazione memoria foto precedente");
        free(last_photo_buffer);
        last_photo_buffer = NULL;
        last_photo_size = 0;
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
        xSemaphoreGive(camera_mutex);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Frame fresco acquisito: %d bytes", fb->len);

    // Alloca memoria per salvare la foto
    last_photo_buffer = (uint8_t *)malloc(fb->len);
    if (last_photo_buffer == NULL)
    {
        ESP_LOGE(TAG, "Errore allocazione memoria per foto");
        esp_camera_fb_return(fb);
        xSemaphoreGive(camera_mutex);
        return ESP_ERR_NO_MEM;
    }

    // Copia i dati
    //sposta fb->len byte dal buffer fb->buf (memoria temporanea) al buffer last_photo_buffer (memoria permanente)
    memcpy(last_photo_buffer, fb->buf, fb->len);
    last_photo_size = fb->len;
    last_photo_timestamp = esp_timer_get_time() / 1000000;

    // Restituisci il frame buffer
    esp_camera_fb_return(fb);

    ESP_LOGI(TAG, "Foto salvata: %d bytes", last_photo_size);

    xSemaphoreGive(camera_mutex); // rilascia il mutex, permettendo a altri task di accedere alla fotocamera
    return ESP_OK;
}

// Funzione helper per ottenere width e height di una risoluzione
static void get_resolution_dimensions(framesize_t size, int* width, int* height) {
    *width = resolution_map[current_resolution_index].width;
    *height = resolution_map[current_resolution_index].height;
    return;
}

// Inizializza fotocamera e mutex corrispondente per mutua esclusione
static esp_err_t camera_init(void)
{
    ESP_LOGI(TAG, "Inizializzazione fotocamera ESP32CAM...");

    // Crea un semaforo mutex (inizializzato ad 1) per l'accesso thread-safe sulla fotocamera
    camera_mutex = xSemaphoreCreateMutex();
    if (camera_mutex == NULL)
    {
        ESP_LOGE(TAG, "Errore creazione mutex fotocamera");
        return ESP_FAIL;
    }

    // Inizializza la fotocamera
    esp_err_t ret = esp_camera_init(&camera_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x: %s", ret, esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "Camera inizializzata con successo");
    }

    return ESP_OK;
}




//Funzioni di handler

//handler per ottenere la risoluzione corrente
static esp_err_t get_current_resolution(httpd_req_t *req)
{
    int width, height;
    get_resolution_dimensions(current_framesize, &width, &height);

    // Invia risposta JSON
    char response[128];
    snprintf(response, sizeof(response), "{\"width\":%d,\"height\":%d}", width, height);
    httpd_resp_set_type(req, "application/json"); 
    httpd_resp_send(req, response, strlen(response));

    return ESP_OK;
}

//handler per incrementare la risoluzione della fotocamera
static esp_err_t increment_resolution(httpd_req_t *req)
{
    //attende 5 secondi e prendere il mutex, se non riesce dopo 5sec ritorna errore
    if (xSemaphoreTake(camera_mutex, pdMS_TO_TICKS(5000)) != pdTRUE)
        {
            ESP_LOGE(TAG, " Timeout acquisizione mutex fotocamera");
            return ESP_ERR_TIMEOUT;
        }
        
        // Leggi il parametro dalla query string
        char query[50];
        httpd_req_get_url_query_str(req, query, sizeof(query));
        
        char direction_str[10];
        int direction = 1; // default incremento
        
        if (httpd_query_key_value(query, "direction", direction_str, sizeof(direction_str)) == ESP_OK) {
            direction = atoi(direction_str);
        }

        // Cambia l'indice in base alla direzione
        if (direction == 0) {
            // Decrementa
            current_resolution_index--;
            if (current_resolution_index < 0) {
                current_resolution_index = sizeof(resolution_map) / sizeof(resolution_map[0]) - 1;
            }
        } else {
            // Incrementa
            current_resolution_index++;
            if (current_resolution_index >= sizeof(resolution_map) / sizeof(resolution_map[0])) {
                current_resolution_index = 0;
            }
        }
    
        // Imposta la nuova risoluzione
        framesize_t new_framesize = resolution_map[current_resolution_index].framesize;
        current_framesize = new_framesize; 
        camera_config.frame_size = new_framesize;
    
        ESP_LOGI(TAG, "Direction: %d, Current index: %d", direction, current_resolution_index);

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
    esp_err_t ret2 = esp_camera_init(&camera_config); // riavvio con nuova configurazione
    if (ret2 != ESP_OK) {
    ESP_LOGE(TAG, "Errore riavvio camera: %s", esp_err_to_name(ret2));
    }
    else
    {
        ESP_LOGI(TAG, "Camera riavviata con successo");
    }

    xSemaphoreGive(camera_mutex);

    // Ottieni dimensioni della risoluzione corrente
    int width, height;
    get_resolution_dimensions(current_framesize, &width, &height);
    ESP_LOGI(TAG, "Risoluzione impostata: %dx%d", width, height);

    // Invia risposta JSON
    char response[128];
    snprintf(response, sizeof(response), "{\"width\":%d,\"height\":%d}", width, height);
    httpd_resp_send(req, response, strlen(response));

    return ESP_OK;
}

// Handler per la pagina principale
static esp_err_t root_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Richiesta pagina principale");

    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");

    // Calcola la dimensione del file HTML embedded
    size_t html_size = main_page_html_end - main_page_html_start;
    
    // Invia l'HTML dall'ESP32 al browser
    esp_err_t ret = httpd_resp_send(req, (const char *)main_page_html_start, html_size);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Errore invio pagina principale: %s", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "Pagina principale inviata (%zu bytes)", html_size);
    }

    return ret;
}

// Handler per lo scatto foto
static esp_err_t capture_get_handler(httpd_req_t *req)
    {
        ESP_LOGI(TAG, "Richiesta scatto foto");

        // Scatta la foto
        esp_err_t ret = camera_capture_photo();
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Errore scatto foto: %s", esp_err_to_name(ret));
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Errore scatto foto");
            return ESP_FAIL;
        }

       // Invia risposta JSON invece di redirect
        char response[128];
        snprintf(response, sizeof(response), "{\"success\":true}");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response, strlen(response));

        return ESP_OK;
    }

// Handler per la visualizzazione foto
static esp_err_t photo_get_handler(httpd_req_t *req)
    {
        ESP_LOGI(TAG, "Richiesta visualizzazione foto");

    uint8_t *buffer;
    size_t size;

    esp_err_t ret = camera_get_last_photo(&buffer, &size);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Nessuna foto disponibile: %s", esp_err_to_name(ret));
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Nessuna foto disponibile");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Invio foto: %d bytes", size);

    // Imposta header per evitare caching
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");

    // Invia la foto al browser per visualizzarla
    ret = httpd_resp_send(req, (const char *)buffer, size);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Foto inviata completamente: %d bytes", size);
    }
    else
    {
        ESP_LOGE(TAG, "Errore invio foto: %s", esp_err_to_name(ret));
    }

    return ret;
}

// Handler per inferenza
static esp_err_t inference_post_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Richiesta inferenza ricevuta");
    
    // Scatta una nuova foto
    if (camera_capture_photo() != ESP_OK) {
        ESP_LOGE(TAG, "Errore durante lo scatto della foto");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Errore camera");
        return ESP_FAIL;
    }
    
    // Ottieni i dati della foto
    uint8_t *photo_buffer;
    size_t photo_size;
    if (camera_get_last_photo(&photo_buffer, &photo_size) != ESP_OK) {
        ESP_LOGE(TAG, "Nessuna foto disponibile per l'inferenza");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Nessuna foto");
        return ESP_FAIL;
    }
    
    // Esegui inferenza
    ESP_LOGI(TAG, "Avvio inferenza su immagine di %zu bytes", photo_size);
    inference_result_t result;
    if (!inference_process_image(photo_buffer, photo_size, &result)) {
        ESP_LOGE(TAG, "Errore durante l'inferenza - photo_size: %zu bytes, photo_buffer: %p", 
                 photo_size, (void*)photo_buffer);
        ESP_LOGE(TAG, "Da controllare: 1) Sistema inferenza inizializzato 2) Dati JPEG validi 3) Memoria disponibile");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Errore inferenza");
        return ESP_FAIL;
    }
    
    // Prepara risposta JSON
    char response[512];
    snprintf(response, sizeof(response), 
        "{\"face_detected\":%s,\"confidence\":%.3f,\"inference_time_ms\":%lu,\"memory_used_kb\":%lu,\"bounding_box\":[%ld,%ld,%ld,%ld],\"num_faces\":%lu,\"success\":true}",
        result.face_detected ? "true" : "false",
        result.confidence,
        result.inference_time_ms,
        result.memory_used_kb,
        result.bounding_boxes[0],
        result.bounding_boxes[1],
        result.bounding_boxes[2],
        result.bounding_boxes[3],
        result.num_faces);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    
    return ESP_OK;
}

// Tabella degli URI handler
static const httpd_uri_t uri_handlers[] = {
    {.uri = "/", //manda il frontend al browser
     .method = HTTP_GET,
     .handler = root_get_handler,
     .user_ctx = NULL},
    {.uri = "/capture", //salva la foto nel buffer della fotocamera, in last_photo_buffer
     .method = HTTP_GET,
     .handler = capture_get_handler,
     .user_ctx = NULL},
    {.uri = "/photo",
     .method = HTTP_GET,
     .handler = photo_get_handler,
     .user_ctx = NULL},
    {.uri = "/change_resolution",
     .method = HTTP_POST,
     .handler = increment_resolution,
     .user_ctx = NULL},
    {.uri = "/resolution/current",
     .method = HTTP_GET,
     .handler = get_current_resolution,
     .user_ctx = NULL},
    {.uri = "/inference",
     .method = HTTP_POST,
     .handler = inference_post_handler,
     .user_ctx = NULL}};



//Funzione per avviare il webserver

//chiamata da main.cpp, avvia una task che lancia il webserver, inizializza la fotocamera e un suo
//relativo mutex, poi registra tutti le route con relativi handlers
esp_err_t webserver_start(void)
{
    ESP_LOGI(TAG, "Avvio webserver HTTP...");

    // Configurazione server HTTP
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    config.stack_size = 8192;
    config.core_id = tskNO_AFFINITY;

    // Inizializza fotocamera
    esp_err_t ret = camera_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Errore inizializzazione fotocamera");
        return ret;
    }

    // Avvia server HTTP con la sua relativa task, questa task rimarrà poi in background
    //per ricevere e gestire tutte le chiamate HTTP da browser
    ret = httpd_start(&server, &config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Errore avvio server HTTP: %s", esp_err_to_name(ret));
        return ret;
    }

    // Registra handler URI
    for (int i = 0; i < sizeof(uri_handlers) / sizeof(uri_handlers[0]); i++)
    {
        ret = httpd_register_uri_handler(server, &uri_handlers[i]);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Errore registrazione handler %s: %s",
                     uri_handlers[i].uri, esp_err_to_name(ret));
            httpd_stop(server);
            return ret;
        }
    }

    ESP_LOGI(TAG, "Webserver avviato con successo");

    return ESP_OK;
}
