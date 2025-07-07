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

   // Funzione per impostare l'IP del webserver
   void webserver_set_ip(const char* ip_address);

#ifdef __cplusplus
}
#endif

#endif // WEBSERVER_H