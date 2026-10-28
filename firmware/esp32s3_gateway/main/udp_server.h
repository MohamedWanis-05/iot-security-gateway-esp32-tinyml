#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#include "net_iface.h"

/* Starts one UDP server task (port 5005) bound to the IP address of `iface`.
 * Call once per interface. `iface` is copied, so it may be a local variable. */
void udp_server_start(const net_iface_t *iface);

#endif
