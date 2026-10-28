#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include "esp_netif.h"

/* Connects to the configured access point. Blocks until connected or failed.
 * Returns true if an IP address was obtained. */
bool wifi_manager_start(void);

/* Netif handle of the Wi-Fi station interface (NULL before wifi_manager_start()). */
esp_netif_t *wifi_manager_get_netif(void);

#endif
