#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include "net_iface.h"

/* Starts one TCP server task (port 5006) bound to the IP address of `iface`.
 * Call once per interface. `iface` is copied, so it may be a local variable. */
void tcp_server_start(const net_iface_t *iface);

#endif
