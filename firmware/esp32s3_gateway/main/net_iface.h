#ifndef NET_IFACE_H
#define NET_IFACE_H

#include "esp_netif.h"

/*
 * Describes one network interface of the gateway.
 * Every server task (UDP / TCP) is started once per interface and binds to
 * that interface's own IP address, so each interface has its own sockets,
 * its own flow window and its own DST_IP in the extracted features.
 */
typedef struct {
    char         name[8];   /* "WIFI" or "ETH" - used in logs and task names */
    esp_netif_t *netif;     /* handle created by wifi_manager / eth_manager  */
} net_iface_t;

#endif
