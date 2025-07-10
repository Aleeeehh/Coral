#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "esp_err.h"
#include "esp_http_server.h"
#include "camera.h"

#ifdef __cplusplus
extern "C" {
#endif

// Struttura per il webserver (classe C-style)
typedef struct {
    httpd_handle_t server;
    camera_t camera;
    char current_ip[16];
    bool initialized;
    bool running;
} webserver_t;

/**
 * @brief Inizializza il webserver
 * @param ws Puntatore alla struttura webserver
 * @return ESP_OK se l'inizializzazione è riuscita
 */
esp_err_t webserver_init(webserver_t *ws);

/**
 * @brief Inizializza il webserver (versione legacy senza parametri)
 * @return ESP_OK se l'inizializzazione è riuscita
 */
esp_err_t webserver_init_legacy(void);

/**
 * @brief Avvia il webserver HTTP
 * @param ws Puntatore alla struttura webserver
 * @return ESP_OK se l'avvio è riuscito
 */
esp_err_t webserver_start(webserver_t *ws);

/**
 * @brief Avvia il webserver HTTP (versione con istanza)
 * @param ws Puntatore alla struttura webserver
 * @return ESP_OK se l'avvio è riuscito
 */
esp_err_t webserver_start_instance(webserver_t *ws);

/**
 * @brief Avvia il webserver HTTP (versione legacy senza parametri)
 * @return ESP_OK se l'avvio è riuscito
 */
esp_err_t webserver_start_legacy(void);

/**
 * @brief Ferma il webserver HTTP
 * @param ws Puntatore alla struttura webserver
 * @return ESP_OK se l'arresto è riuscito
 */
esp_err_t webserver_stop(webserver_t *ws);

/**
 * @brief Deinizializza il webserver
 * @param ws Puntatore alla struttura webserver
 * @return ESP_OK se la deinizializzazione è riuscita
 */
esp_err_t webserver_deinit(webserver_t *ws);

/**
 * @brief Imposta l'IP del webserver
 * @param ws Puntatore alla struttura webserver
 * @param ip_address Indirizzo IP da impostare
 */
void webserver_set_ip(webserver_t *ws, const char* ip_address);

/**
 * @brief Imposta l'IP del webserver (versione con istanza)
 * @param ws Puntatore alla struttura webserver
 * @param ip_address Indirizzo IP da impostare
 */
void webserver_set_ip_instance(webserver_t *ws, const char* ip_address);

/**
 * @brief Imposta l'IP del webserver (versione legacy senza parametri)
 * @param ip_address Indirizzo IP da impostare
 */
void webserver_set_ip_legacy(const char* ip_address);

/**
 * @brief Ottiene l'IP corrente del webserver
 * @param ws Puntatore alla struttura webserver
 * @return Puntatore all'IP corrente
 */
const char* webserver_get_ip(webserver_t *ws);

/**
 * @brief Controlla se il webserver è in esecuzione
 * @param ws Puntatore alla struttura webserver
 * @return true se il webserver è in esecuzione
 */
bool webserver_is_running(webserver_t *ws);

#ifdef __cplusplus
}
#endif

#endif // WEBSERVER_H