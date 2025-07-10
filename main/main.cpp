#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "webserver.h"
#include "inference.h"
#include "camera.h"

#define WIFI_SSID "Iphone di Prato"
#define WIFI_PASS "Ciaoo111"

static const char *TAG = "MAIN";

// Inizializza la fotocamera globalmente
camera_t g_camera;

// "ESP_ERROR_CHECK(x)" = esegui x normalmente, e se fallisce, riavvia l'esp32

// Un "event group" è un oggetto di FreeRTOS, che gestisce la comunicazione tra task
// utilizzando i bit di flag per segnalare certi stati (bit1="wifi connesso", bit2= "IP ottenuto", etc..)
static EventGroupHandle_t wifi_event_group; 
const int WIFI_CONNECTED_BIT = BIT0; // bit flag per intendere "wifi connesso" (se il bit flag è BIT0, allora il wifi è connesso)

// Gestore di task, può essere usato per sospendere, modificare,riprendere, eliminare, o ottenere informazioni su di esso
static TaskHandle_t webserver_task_handle = NULL;


// WiFi event handler, gestisce tre eventi diversi
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)  //valori interni al framework ESP-IDF, riempiti quando si verifica un evento
    {
        esp_wifi_connect();  //quando il wifi si avvia, tenta la connessione 
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(TAG, "Connessione WiFi persa, tentativo di riconnessione...");
        esp_wifi_connect(); //se la connessione si perde, riprova riconnettersi e cancella il bit di connessione
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data; // quando ottiene un IP, imposta il bit di connessione
        ESP_LOGI(TAG, "IP ottenuto:" IPSTR, IP2STR(&event->ip_info.ip));
        
        // Converte l'IP in stringa e lo passa al webserver
        char ip_str[16];
        snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&event->ip_info.ip));
        webserver_set_ip_legacy(ip_str); // Usa la versione legacy senza parametri
        
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}



// Implementazione del task per il webserver
static void webserver_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Task webserver avviato");

    // Aspetta che il bit di connessione sia impostato nell'event group, lo fa aspettare(bloccante) fino a che non è impostato
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);

    // Inizializza il sistema di inferenza dopo la connessione WiFi (potremmo creare una task dedicata)
    ESP_LOGI(TAG, "Inizializzazione sistema di inferenza...");
    if (!inference_init_legacy()) {
        ESP_LOGE(TAG, "Errore inizializzazione sistema di inferenza");
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Sistema di inferenza inizializzato con successo");
    

    // Inizializza il webserver
    esp_err_t ret = webserver_init_legacy();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Errore inizializzazione webserver");
        vTaskDelete(NULL);
        return;
    }

    //lancia il webserver
    webserver_start_legacy();

    // Poi termina il task, il task del webserver è ora indipendente
    vTaskDelete(NULL);
}


static void start_webserver(){
    // Crea event group per WiFi
    wifi_event_group = xEventGroupCreate(); //crea un event group per la connessione WiFi
    
    // Inizializzazione WiFi
    ESP_ERROR_CHECK(esp_netif_init()); //inizializza il network interface
    ESP_ERROR_CHECK(esp_event_loop_create_default()); //crea un loop per gestire gli eventi
    esp_netif_create_default_wifi_sta(); //crea un network interface per la connessione WiFi, configurandola come "station" (client)

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); //crea una configurazione wifi di default
    ESP_ERROR_CHECK(esp_wifi_init(&cfg)); //inizializza il driver wifi dell'esp32, usando tale configurazione
    //NON STA ANCORA AVVIANDO LA CONNESSIONE!

    // Registriamo un medesimo gestore eventi per il WiFi (sia di connessione che di assegnazione IP)
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, //relativo a eventi di connessione/disconnessione wifi
                                                        ESP_EVENT_ANY_ID, 
                                                        &event_handler, 
                                                        NULL, 
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, //relativo a eventi di assegnazione IP
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));

    // Configura WiFi
    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.sta.ssid, WIFI_SSID);
    strcpy((char*)wifi_config.sta.password, WIFI_PASS);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); //imposta il wifi in modalità station (client)
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config)); //imposta la configurazione wifi
    ESP_ERROR_CHECK(esp_wifi_start()); //avvia il wifi

    ESP_LOGI(TAG, "Connessione WiFi in corso...");

    // Crea un task per il webserver, eseguendolo sul core 0, con priorità 2 (la più alta), assegnandogli 65536 byte di stack e usa webserver_task come handler
    xTaskCreatePinnedToCore(webserver_task, "webserver_task", 65536, NULL, 2, &webserver_task_handle, 0);

    ESP_LOGI(TAG, "Tutti i task creati e avviati");
    ESP_LOGI(TAG, "Webserver disponibile su http://%s", IPSTR);
}


static void cli_task(void *pvParameters){
    printf("===========================\n");
    printf("INTERFACCIA A RIGA DI COMANDO\n"); 
    printf("===========================\n");
    printf("h: mostra i comandi disponibili\n");
    printf("i: Inizializza la fotocamera e il sistema di inferenza\n");
    printf("d: Deinizializza la fotocamera e il sistema di inferenza\n");
    printf("+: aumenta risoluzione fotocamera\n");
    printf("-: riduci risoluzione fotocamera\n");
    printf("w: Avvia il webserver per web UI\n");
    printf("s: Scatta foto ed esegui inferenza face detection\n");
    printf("e: Esci\n");
    printf("RICORDA DI DEINIZIALIZZARE CAMERA E SISTEMA INFERENZA PRIMA DI LANCIARE WEBSERVER!\n");
    printf("===========================\n");
    printf("Inserisci un comando:\n");
    int command;
    while (true) {
        command = getchar();
        if (command == 'w') {
            printf("Avvio webserver per web UI all'IP 172.20.10.3...\n");
            start_webserver();
        }
        else if (command == 's') {
            printf("Scatto foto ed eseguo inferenza face detection...\n");
            camera_capture_and_inference(&g_camera, NULL);
        }
        else if (command == 'i'){
            printf("Inizializza il sistema di inferenza...\n");
            inference_init_legacy();
            camera_init(&g_camera);
        }
        else if (command == 'd') {
            printf("Deinizializza la fotocamera e il sistema di inferenza...\n");
            camera_deinit(&g_camera);
            inference_deinit_legacy();
        }
        else if (command == '+') {
            printf("Aumenta risoluzione fotocamera...\n");
            camera_change_resolution(&g_camera, 1);
        }
        else if (command == '-') {
            printf("Riduci risoluzione fotocamera...\n");
            camera_change_resolution(&g_camera, -1);
        }
        else if (command == 'h') {
            printf("===========================\n");
            printf("INTERFACCIA A RIGA DI COMANDO\n"); 
            printf("===========================\n");
            printf("h: mostra i comandi disponibili\n");
            printf("i: Inizializza la fotocamera e il sistema di inferenza\n");
            printf("d: Deinizializza la fotocamera e il sistema di inferenza\n");
            printf("+: aumenta risoluzione fotocamera\n");
            printf("-: riduci risoluzione fotocamera\n");
            printf("w: Avvia il webserver per web UI\n");
            printf("s: Scatta foto ed esegui inferenza face detection\n");
            printf("e: Esci\n");
            printf("RICORDA DI DEINIZIALIZZARE CAMERA E SISTEMA INFERENZA PRIMA DI LANCIARE WEBSERVER!\n");
            printf("===========================\n");
            printf("Inserisci un comando:\n");
        }
        else if (command == 'e') {
            printf("Uscita...\n");
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }   
    vTaskDelete(NULL); // Termina la task

}

//inizio dell'applicazione
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Avvio ESP32-S3 Ai Camera con ESP-IDF e FreeRTOS");

    //visualizza la PSRAM attuale
    ESP_LOGI("PSRAM", "Detected PSRAM: %zu bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    
    // Informazioni aggiuntive sulla memoria
    ESP_LOGI("MEMORY", "Free heap: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI("MEMORY", "Largest free block: %lu bytes", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    ESP_LOGI("MEMORY", "Free PSRAM: %zu bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    // Inizializza NVS = "Non volatil storage" (Memoria flash dell'esp32)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) //se c'è un problema con lo storage
    {
        ESP_ERROR_CHECK(nvs_flash_erase()); //prova a svuotare la flash
        ret = nvs_flash_init(); //prova a rifare l'inizializzazione dello storage
    }
    ESP_ERROR_CHECK(ret); //se ret != ESP_OK, riavvia l'esp32

    //Crea task per la CLI
    xTaskCreate(cli_task, "cli_task", 4096, NULL, 1, NULL);


    
}