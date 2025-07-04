#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
 * @brief Avvia il webserver HTTP
 * @return ESP_OK se l'avvio è riuscito
 */
    esp_err_t webserver_start(void);

    /**
 * @brief Ferma il webserver HTTP
 * @return ESP_OK se lo stop è riuscito
 */
    esp_err_t webserver_stop(void);

    /**
 * @brief Gestisce le richieste HTTP (da chiamare periodicamente)
 */
    void webserver_handle_requests(void);

    /**
 * @brief Ottiene l'handle del server HTTP
 * @return Handle del server o NULL se non inizializzato
 */
    httpd_handle_t webserver_get_handle(void);

    // Funzione per impostare l'IP del webserver
    void webserver_set_ip(const char* ip_address);

#ifdef __cplusplus
}
#endif

#endif // WEBSERVER_H