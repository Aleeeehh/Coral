#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

   /**
 * @brief Inizializza il LED flash dell'ESP32CAM
 * @return ESP_OK se l'inizializzazione è riuscita
 */
   esp_err_t led_init(void);

   /**
 * @brief Accende il LED flash
 */
   void led_on(void);

   /**
 * @brief Spegne il LED flash
 */
   void led_off(void);

   /**
 * @brief Fa lampeggiare il LED flash
 */
   void led_blink(void);

   /**
 * @brief Imposta lo stato del LED
 * @param state true per accendere, false per spegnere
 */
   void led_set_state(bool state);

#ifdef __cplusplus
}
#endif

#endif // LED_CONTROL_H