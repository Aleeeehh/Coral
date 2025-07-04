#include "webserver.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"

static const char *TAG = "WEBSERVER";

// Dichiarazioni delle funzioni statiche
static esp_err_t camera_init(void);
static esp_err_t camera_capture_photo(void);
static esp_err_t camera_get_last_photo(uint8_t **buffer, size_t *size);

// Variabili globali per la fotocamera
static uint8_t *last_photo_buffer = NULL; //buffer di 8 bit per la foto
static size_t last_photo_size = 0; //dimensione della foto, usato size_t per portabilità (valori diversi in base a tipo di esp, in questo caso "32 bit")
static uint32_t last_photo_timestamp = 0; //timestamp della foto
static SemaphoreHandle_t camera_mutex = NULL; //semaforo mutex per la fotocamera

// Variabile globale per l'IP
static char current_ip[16] = "0.0.0.0"; // IP di default

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
  .frame_size    = FRAMESIZE_QVGA, //FRAMESIZE_QXGA 3MP di risoluzione, altrimenti usa FRAMESIZE_QVGA
  .jpeg_quality  = 10, //aumento qualità
  .fb_count      = 1,
  .fb_location   = CAMERA_FB_IN_DRAM, //da implementare in PSRAM
  .grab_mode     = CAMERA_GRAB_WHEN_EMPTY,
  .sccb_i2c_port = 0
};


// HTML per la pagina principale
static const char *main_page_html = R"rawliteral(
                                    <!DOCTYPE html>
                                    <html>
                                        <head>
                                        <title> ESP32 - CAM</title>
                                        <meta charset = 'UTF-8'>
                                        <style>
                                            body
{
    font-family : Arial, sans-serif;
    text-align : center;
margin:
    20px;
    background-color : #f0f0f0;
}
.container
{
    max-width : 800px;
margin:
    0 auto;
    background-color : white;
padding:
    20px;
    border-radius : 10px;
    box-shadow : 0 2px 10px rgba(0, 0, 0, 0.1);
}
.photo-container
{
margin:
    20px;
padding:
    10px;
border:
    2px solid #ccc;
display:
    inline-block;
    border-radius : 5px;
}
.stream-container
{
margin:
    20px;
padding:
    10px;
border:
    2px solid #0066cc;
display:
    inline-block;
    border-radius : 5px;
}
button
{
padding:
    15px 30px;
    font-size : 18px;
margin:
    10px;
border:
    none;
cursor:
    pointer;
    border-radius : 5px;
transition:
    background-color 0.3s;
}
.photo-btn
{
    background-color : #4CAF50;
color:
    white;
}
.photo-btn : hover
{
    background-color : #45a049;
}
.stream-btn
{
    background-color : #0066cc;
color:
    white;
}
.stream-btn : hover
{
    background-color : #0052a3;
}
.status
{
margin:
    10px;
padding:
    10px;
    border-radius : 5px;
    font-weight : bold;
}
.status.connected
{
    background-color : #d4edda;
color:
# 155724;
    border:
        1px solid #c3e6cb;
    }
    .status.disconnected
    {
        background-color : #f8d7da;
    color:
# 721c24;
    border:
        1px solid #f5c6cb;
    }
    </style>
        <script>
            window.onload = function()
    {
        updatePhoto();
    };

    function updatePhoto()
    {
        var img = document.getElementById('photo');
        if (img)
        {
            console.log('Aggiornamento foto...');
            img.src = '/photo?t=' + Date.now();
            img.onload = function()
            {
                console.log('✅ Foto caricata con successo');
                img.style.display = 'block';
            };
            img.onerror = function()
            {
                console.log('❌ Errore caricamento foto');
                img.style.display = 'none';
            };
        }
        else
        {
            console.log('❌ Elemento img#photo non trovato');
        }
    }

    function updateStatus()
    {
        fetch('/status')
            .then(response => response.json())
            .then(data => {
                var statusDiv = document.getElementById('status');
                if (data.connected)
                {
                    statusDiv.className = 'status connected';
                    statusDiv.textContent = '✅ Connesso - IP: ' + data.ip;
                }
                else
                {
                    statusDiv.className = 'status disconnected';
                    statusDiv.textContent = '❌ Disconnesso';
                }
            })
            .catch(error => {
                console.error('Errore aggiornamento status:', error);
            });
    }

    // Aggiorna status ogni 5 secondi
    setInterval(updateStatus, 5000);
    updateStatus();
    </script>
</head>
<body>
    <div class="container">
        <h1>📷 ESP32-CAM</h1>
        <div id="status" class="status">Caricamento...</div>
        <p>
            <a href="/capture"><button class="photo-btn">📸 Scatta Foto</button></a>
        </p>
        <div id="photo-container" class="photo-container">
            <h3>Ultima Foto Scattata:</h3>
            <img id="photo" style="max-width: 100%; max-height: 400px;">
        </div>
    </div>
</body>
</html>
)rawliteral";

// Handler per la pagina principale
static esp_err_t root_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "🏠 Richiesta pagina principale");

    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");

    //invia l'html dal esp32 al browser 
    esp_err_t ret = httpd_resp_send(req, main_page_html, strlen(main_page_html));
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "❌ Errore invio pagina principale: %s", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "✅ Pagina principale inviata");
    }

    return ret;
}

    // Handler per lo scatto foto
    static esp_err_t capture_get_handler(httpd_req_t *req)
    {
        ESP_LOGI(TAG, "📸 Richiesta scatto foto");

        // Scatta la foto
        esp_err_t ret = camera_capture_photo();
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "❌ Errore scatto foto: %s", esp_err_to_name(ret));
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Errore scatto foto");
            return ESP_FAIL;
        }

        // Invia pagina di conferma con redirect
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/");
        httpd_resp_send(req, NULL, 0);

        return ESP_OK;
    }

    // Handler per la visualizzazione foto
    static esp_err_t photo_get_handler(httpd_req_t *req)
    {
        ESP_LOGI(TAG, "🖼️ Richiesta visualizzazione foto");

    uint8_t *buffer;
    size_t size;

    esp_err_t ret = camera_get_last_photo(&buffer, &size);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "❌ Nessuna foto disponibile: %s", esp_err_to_name(ret));
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Nessuna foto disponibile");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "📤 Invio foto: %d bytes", size);

    // Imposta header per evitare caching
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");

    // Invia i dati
    ret = httpd_resp_send(req, (const char *)buffer, size);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "✅ Foto inviata completamente: %d bytes", size);
    }
    else
    {
        ESP_LOGE(TAG, "❌ Errore invio foto: %s", esp_err_to_name(ret));
    }

    return ret;
}

// Handler per lo status del sistema
static esp_err_t status_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "📊 Richiesta status sistema");

    // Status con IP reale
    char json_response[256];
    snprintf(json_response, sizeof(json_response),
             "{\"connected\":true,\"ip\":\"%s\",\"uptime\":%llu}",
             current_ip, esp_timer_get_time() / 1000000);

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json_response, strlen(json_response));

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "✅ Status inviato: %s", json_response);
    }

    return ret;
}

// Funzioni per la gestione della fotocamera
static esp_err_t camera_init(void)
{
    ESP_LOGI(TAG, "🔧 Inizializzazione fotocamera ESP32CAM...");

    // Crea un semaforo mutex (inizializzato ad 1) per l'accesso thread-safe sulla fotocamera
    camera_mutex = xSemaphoreCreateMutex();
    if (camera_mutex == NULL)
    {
        ESP_LOGE(TAG, "❌ Errore creazione mutex fotocamera");
        return ESP_FAIL;
    }

    // Inizializza la fotocamera
    esp_err_t ret = esp_camera_init(&camera_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Camera init failed with error 0x%x: %s", ret, esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "✅ Camera inizializzata con successo");
    }

    return ESP_OK;
}

static esp_err_t camera_capture_photo(void)
{
    ESP_LOGI(TAG, "📸 Acquisizione foto...");

    //attende 5 secondi e prendere il mutex, se non riesce dopo 5sec ritorna errore
    if (xSemaphoreTake(camera_mutex, pdMS_TO_TICKS(5000)) != pdTRUE)
    {
        ESP_LOGE(TAG, "❌ Timeout acquisizione mutex fotocamera");
        return ESP_ERR_TIMEOUT;
    }

    // Libera memoria foto precedente
    if (last_photo_buffer != NULL)
    {
        ESP_LOGI(TAG, "🗑️ Liberazione memoria foto precedente");
        free(last_photo_buffer);
        last_photo_buffer = NULL;
        last_photo_size = 0;
    }

    // Scarta il primo frame (potrebbe essere vecchio)
    ESP_LOGI(TAG, "🗑️ Scarto primo frame (potrebbe essere vecchio)...");
    camera_fb_t *fb_old = esp_camera_fb_get(); //acquisisce il frame 
    if (fb_old) {
        ESP_LOGI(TAG, "📊 Frame vecchio scartato: %d bytes", fb_old->len);
        esp_camera_fb_return(fb_old); //riempie il buffer della fotocamera col frame 
    }

    // Piccolo delay per permettere alla fotocamera di acquisire un nuovo frame
    vTaskDelay(pdMS_TO_TICKS(100));

    // Acquisisci il frame fresco
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
        ESP_LOGE(TAG, "❌ Errore acquisizione frame fotocamera");
        xSemaphoreGive(camera_mutex);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "📊 Frame fresco acquisito: %d bytes", fb->len);

    // Alloca memoria per salvare la foto
    last_photo_buffer = (uint8_t *)malloc(fb->len);
    if (last_photo_buffer == NULL)
    {
        ESP_LOGE(TAG, "❌ Errore allocazione memoria per foto");
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

    ESP_LOGI(TAG, "✅ Foto salvata: %d bytes", last_photo_size);

    xSemaphoreGive(camera_mutex); // rilascia il mutex, permettendo a altri task di accedere alla fotocamera
    return ESP_OK;
}

//TODO: questa funzione è da rivedere, vedo sempre una foto vecchia, e non la corrente
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

// Variabili globali per il server HTTP
static httpd_handle_t server = NULL;

// Tabella degli URI handler
static const httpd_uri_t uri_handlers[] = {
    {.uri = "/",
     .method = HTTP_GET,
     .handler = root_get_handler,
     .user_ctx = NULL},
    {.uri = "/capture",
     .method = HTTP_GET,
     .handler = capture_get_handler,
     .user_ctx = NULL},
    {.uri = "/photo",
     .method = HTTP_GET,
     .handler = photo_get_handler,
     .user_ctx = NULL},
    {.uri = "/status",
     .method = HTTP_GET,
     .handler = status_get_handler,
     .user_ctx = NULL}};

esp_err_t webserver_start(void)
{
    ESP_LOGI(TAG, "🚀 Avvio webserver HTTP...");

    // Configurazione server HTTP
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    config.stack_size = 8192;
    config.core_id = tskNO_AFFINITY;

    // Inizializza fotocamera
    esp_err_t ret = camera_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "❌ Errore inizializzazione fotocamera");
        return ret;
    }

    // Avvia server HTTP
    ret = httpd_start(&server, &config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "❌ Errore avvio server HTTP: %s", esp_err_to_name(ret));
        return ret;
    }

    // Registra handler URI
    for (int i = 0; i < sizeof(uri_handlers) / sizeof(uri_handlers[0]); i++)
    {
        ret = httpd_register_uri_handler(server, &uri_handlers[i]);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "❌ Errore registrazione handler %s: %s",
                     uri_handlers[i].uri, esp_err_to_name(ret));
            httpd_stop(server);
            return ret;
        }
    }

    ESP_LOGI(TAG, "✅ Webserver avviato con successo");

    return ESP_OK;
}

esp_err_t webserver_stop(void)
{
    if (server != NULL)
    {
        ESP_LOGI(TAG, "🛑 Arresto webserver...");

        // Libera memoria foto
        if (last_photo_buffer != NULL)
        {
            free(last_photo_buffer);
            last_photo_buffer = NULL;
            last_photo_size = 0;
        }

        // Elimina mutex
        if (camera_mutex != NULL)
        {
            vSemaphoreDelete(camera_mutex);
            camera_mutex = NULL;
        }

        // Ferma server
        esp_err_t ret = httpd_stop(server);
        server = NULL;

        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "✅ Webserver arrestato");
        }

        return ret;
    }

    return ESP_OK;
}

void webserver_handle_requests(void)
{
    // Il server HTTP gestisce automaticamente le richieste
    // Questa funzione può essere usata per logica aggiuntiva se necessario
}

httpd_handle_t webserver_get_handle(void)
{
    return server;
}

// Funzione per impostare l'IP del webserver
void webserver_set_ip(const char* ip_address)
{
    if (ip_address != NULL && strlen(ip_address) < sizeof(current_ip)) {
        strcpy(current_ip, ip_address);
        ESP_LOGI(TAG, "🌐 IP del webserver impostato a: %s", current_ip);
    }
}