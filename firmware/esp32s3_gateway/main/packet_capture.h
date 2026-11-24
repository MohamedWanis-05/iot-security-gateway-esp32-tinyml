#pragma once

#include "esp_eth.h"
#include "esp_netif.h"

esp_err_t packet_capture_start(esp_eth_handle_t handle, esp_eth_mac_t *mac, esp_netif_t *netif);
