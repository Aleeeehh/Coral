#include "webserver.h"
#include "inference.h"
#include "camera.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <fstream>
#include <sstream>

static const char *TAG = "WEBSERVER";

// Dichiarazioni delle funzioni statiche
static esp_err_t inference_post_handler(httpd_req_t *req);

// Variabile globale per il webserver (singleton per compatibilità)
static webserver_t g_webserver;

// Funzione helper per ottenere l'istanza globale (per compatibilità con handler esistenti)
static webserver_t* get_webserver_instance(void) {
    return &g_webserver;
}

// Dichiarazioni esterne per il file HTML embedded
extern const uint8_t main_page_html_start[] asm("_binary_main_page_html_start");
extern const uint8_t main_page_html_end[] asm("_binary_main_page_html_end");



//Funzioni di handler per HTTP

//handler per ottenere la risoluzione corrente
static esp_err_t get_current_resolution(httpd_req_t *req)
{
    webserver_t *ws = get_webserver_instance();
    int width, height;
    camera_get_current_resolution(&ws->camera, &width, &height);

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
    webserver_t *ws = get_webserver_instance();
    
    // Leggi il parametro dalla query string
    char query[50];
    httpd_req_get_url_query_str(req, query, sizeof(query));
    
    char direction_str[10];
    int direction = 1; // default incremento
    
    if (httpd_query_key_value(query, "direction", direction_str, sizeof(direction_str)) == ESP_OK) {
        direction = atoi(direction_str);
    }

    // Cambia la risoluzione usando la classe Camera
    esp_err_t ret = camera_change_resolution(&ws->camera, direction);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Errore cambio risoluzione: %s", esp_err_to_name(ret));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Errore cambio risoluzione");
        return ESP_FAIL;
    }

    // Ottieni dimensioni della risoluzione corrente
    int width, height;
    camera_get_current_resolution(&ws->camera, &width, &height);
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
        webserver_t *ws = get_webserver_instance();
        ESP_LOGI(TAG, "Richiesta scatto foto");

        // Scatta la foto usando la classe Camera
        esp_err_t ret = camera_capture_photo(&ws->camera);
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
        webserver_t *ws = get_webserver_instance();
        ESP_LOGI(TAG, "Richiesta visualizzazione foto");

    uint8_t *buffer;
    size_t size;

    esp_err_t ret = camera_get_last_photo(&ws->camera, &buffer, &size);
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
    webserver_t *ws = get_webserver_instance();
    ESP_LOGI(TAG, "Richiesta inferenza ricevuta");
    
    // Scatta una nuova foto
    if (camera_capture_photo(&ws->camera) != ESP_OK) {
        ESP_LOGE(TAG, "Errore durante lo scatto della foto");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Errore camera");
        return ESP_FAIL;
    }
    
    // Ottieni i dati della foto
    uint8_t *photo_buffer;
    size_t photo_size;
    if (camera_get_last_photo(&ws->camera, &photo_buffer, &photo_size) != ESP_OK) {
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
    char response[2048]; // Aumentato per supportare multiple facce
    
    // Costruisci array JSON per tutte le facce
    char faces_array[1024] = "[";
    for (uint32_t i = 0; i < result.num_faces && i < MAX_FACES; i++) {
        // Costruisci array keypoints per questa faccia
        char keypoints_str[256] = "[";
        if (result.faces[i].num_keypoints > 0) {
            for (size_t k = 0; k < result.faces[i].num_keypoints; k++) {
                char temp[16];
                snprintf(temp, sizeof(temp), "%lu", result.faces[i].keypoints[k]);
                strcat(keypoints_str, temp);
                if (k < result.faces[i].num_keypoints - 1) strcat(keypoints_str, ",");
            }
        }
        strcat(keypoints_str, "]");
        
        // Aggiungi questa faccia all'array
        char face_json[512];
        snprintf(face_json, sizeof(face_json),
            "{\"confidence\":%.3f,\"bounding_box\":[%lu,%lu,%lu,%lu],\"keypoints\":%s,\"num_keypoints\":%lu,\"category\":%lu}",
            result.faces[i].confidence,
            result.faces[i].bounding_boxes[0],
            result.faces[i].bounding_boxes[1],
            result.faces[i].bounding_boxes[2],
            result.faces[i].bounding_boxes[3],
            keypoints_str,
            result.faces[i].num_keypoints,
            result.faces[i].category);
        
        strcat(faces_array, face_json);
        if (i < result.num_faces - 1 && i < MAX_FACES - 1) strcat(faces_array, ",");
    }
    strcat(faces_array, "]");
    
    snprintf(response, sizeof(response), 
        "{\"face_detected\":%s,\"inference_time_ms\":%lu,\"num_faces\":%lu,\"faces\":%s,\"success\":true}",
        result.face_detected ? "true" : "false",
        result.full_inference_time_ms,
        result.num_faces,
        faces_array);
    
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



//Funzioni per la classe C-style del webserver

//chiamata da main.cpp, avvia una task che lancia il webserver, inizializza la fotocamera e un suo
//relativo mutex, poi registra tutti le route con relativi handlers
esp_err_t webserver_start_legacy(void)
{
    return webserver_start_instance(&g_webserver);
}

// Funzione wrapper per compatibilità (versione legacy senza parametri)
esp_err_t webserver_init_legacy(void)
{
    return webserver_init(&g_webserver);
}

esp_err_t webserver_init(webserver_t *ws)
{
    if (!ws) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Inizializzazione webserver...");

    // Inizializza struttura
    memset(ws, 0, sizeof(webserver_t));
    strcpy(ws->current_ip, "0.0.0.0");
    ws->initialized = true;

    // Inizializza fotocamera
    esp_err_t ret = camera_init(&ws->camera);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Errore inizializzazione fotocamera");
        return ret;
    }

    ESP_LOGI(TAG, "Webserver inizializzato con successo");
    return ESP_OK;
}

esp_err_t webserver_start_instance(webserver_t *ws)
{
    if (!ws || !ws->initialized) {
        ESP_LOGE(TAG, "Webserver non inizializzato");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Avvio webserver HTTP...");

    // Configurazione server HTTP
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    config.stack_size = 8192;
    config.core_id = tskNO_AFFINITY;

    // Avvia server HTTP
    esp_err_t ret = httpd_start(&ws->server, &config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Errore avvio server HTTP: %s", esp_err_to_name(ret));
        return ret;
    }

    // Registra handler URI
    for (int i = 0; i < sizeof(uri_handlers) / sizeof(uri_handlers[0]); i++)
    {
        ret = httpd_register_uri_handler(ws->server, &uri_handlers[i]);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Errore registrazione handler %s: %s",
                     uri_handlers[i].uri, esp_err_to_name(ret));
            httpd_stop(ws->server);
            return ret;
        }
    }

    ws->running = true;
    printf("===========================\n");
    printf("Webserver avviato con successo\n");
    printf("===========================\n");

    return ESP_OK;
}

esp_err_t webserver_stop(webserver_t *ws)
{
    if (!ws) {
        return ESP_ERR_INVALID_ARG;
    }

    if (ws->running && ws->server) {
        ESP_LOGI(TAG, "Arresto webserver...");
        esp_err_t ret = httpd_stop(ws->server);
        if (ret == ESP_OK) {
            ws->running = false;
            ws->server = NULL;
            ESP_LOGI(TAG, "Webserver arrestato con successo");
        }
        return ret;
    }

    return ESP_OK;
}

esp_err_t webserver_deinit(webserver_t *ws)
{
    if (!ws) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Deinizializzazione webserver...");

    // Ferma il server se in esecuzione
    webserver_stop(ws);

    // Deinizializza fotocamera
    camera_deinit(&ws->camera);

    ws->initialized = false;
    ESP_LOGI(TAG, "Webserver deinizializzato");

    return ESP_OK;
}

const char* webserver_get_ip(webserver_t *ws)
{
    if (!ws) {
        return NULL;
    }
    return ws->current_ip;
}

bool webserver_is_running(webserver_t *ws)
{
    if (!ws) {
        return false;
    }
    return ws->running;
}



//Funzioni helper

// Funzione per impostare l'IP del webserver (versione legacy per compatibilità)
void webserver_set_ip(const char* ip_address)
{
    webserver_set_ip_instance(&g_webserver, ip_address);
}

// Funzione wrapper per compatibilità (versione legacy senza parametri)
void webserver_set_ip_legacy(const char* ip_address)
{
    webserver_set_ip_instance(&g_webserver, ip_address);
}

// Funzione per impostare l'IP del webserver (nuova versione con istanza)
void webserver_set_ip_instance(webserver_t *ws, const char* ip_address)
{
    if (ws && ip_address != NULL && strlen(ip_address) < sizeof(ws->current_ip)) {
        strcpy(ws->current_ip, ip_address);
        ESP_LOGI(TAG, "IP del webserver impostato a: %s", ws->current_ip);
    }
}

