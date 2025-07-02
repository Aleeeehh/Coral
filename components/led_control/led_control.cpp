#include "led_control.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "LED_CONTROL";

#define LED_GPIO GPIO_NUM_4 // LED flash ESP32CAM

esp_err_t led_init(void)
{
    ESP_LOGI(TAG, "Inizializzazione LED flash su GPIO %d", LED_GPIO);

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Errore configurazione GPIO: %s", esp_err_to_name(ret));
        return ret;
    }

    // LED spento all'inizializzazione
    led_off();

    ESP_LOGI(TAG, "✅ LED flash inizializzato");
    return ESP_OK;
}

void led_on(void)
{
    gpio_set_level(LED_GPIO, 0); // LED acceso (logica invertita)
}

void led_off(void)
{
    gpio_set_level(LED_GPIO, 1); // LED spento (logica invertita)
}

void led_blink(void)
{
    led_on();
    vTaskDelay(pdMS_TO_TICKS(100));
    led_off();
}

void led_set_state(bool state)
{
    if (state)
    {
        led_on();
    }
    else
    {
        led_off();
    }
}