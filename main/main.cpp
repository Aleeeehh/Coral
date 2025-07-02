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
#include "nvs_flash.h"

#include "webserver.h"
#include "led_control.h"

static const char *TAG = "MAIN";

// Event group per sincronizzazione
static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;

// Task handles
static TaskHandle_t camera_task_handle = NULL;
static TaskHandle_t webserver_task_handle = NULL;
static TaskHandle_t led_task_handle = NULL;

// WiFi event handler
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(TAG, "Connessione WiFi persa, tentativo di riconnessione...");
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "IP ottenuto:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// Task per gestione LED
static void led_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Task LED avviato");

    while (1)
    {
        led_blink();
        vTaskDelay(pdMS_TO_TICKS(2000)); // Blink ogni 2 secondi
    }
}

// Task per gestione fotocamera
static void camera_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Task fotocamera avviato");

    while (1)
    {
        // Task fotocamera in standby
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Task per gestione webserver
static void webserver_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Task webserver avviato");

    // Aspetta che il WiFi sia connesso
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);

    // Avvia il webserver
    webserver_start();

    while (1)
    {
        webserver_handle_requests();
        vTaskDelay(pdMS_TO_TICKS(10)); // 10ms delay
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "🚀 Avvio ESP32CAM con ESP-IDF e FreeRTOS");

    // Inizializza NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Crea event group per WiFi
    wifi_event_group = xEventGroupCreate();
    /*
    // Inizializza LED
    led_init();
    ESP_LOGI(TAG, "✅ LED inizializzato");

    ESP_LOGI(TAG, "✅ Sistema inizializzato");
*/
    // Inizializza WiFi
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));

    // Configura WiFi
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "Iphone di Prato",
            .password = "Ciaoo111",
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "📡 Connessione WiFi in corso...");

    // Crea event group per sincronizzazione
    wifi_event_group = xEventGroupCreate();

    // Crea task
    xTaskCreatePinnedToCore(led_task, "led_task", 2048, NULL, 1, &led_task_handle, 0);
    xTaskCreatePinnedToCore(camera_task, "camera_task", 8192, NULL, 3, &camera_task_handle, 1);
    xTaskCreatePinnedToCore(webserver_task, "webserver_task", 8192, NULL, 2, &webserver_task_handle, 0);

    ESP_LOGI(TAG, "✅ Tutti i task creati e avviati");
    ESP_LOGI(TAG, "🌐 Webserver disponibile su http://[IP_ESP32]");
}