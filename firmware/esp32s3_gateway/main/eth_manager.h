#ifndef ETH_MANAGER_H
#define ETH_MANAGER_H

#include <stdbool.h>
#include "esp_netif.h"

/* Brings up the W5500 SPI Ethernet interface. Returns true if the driver started.
 * Link state and IP address are reported later through log messages / events. */
bool eth_manager_start(void);

/* Netif handle of the Ethernet interface (NULL before eth_manager_start()). */
esp_netif_t *eth_manager_get_netif(void);

#endif
