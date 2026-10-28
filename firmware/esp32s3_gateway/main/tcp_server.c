#include "tcp_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/sockets.h"

#include "feature_extractor.h"

#define TCP_SERVER_PORT          5006
#define RX_BUFFER_SIZE           128
#define KEEPALIVE_IDLE           5
#define KEEPALIVE_INTERVAL       5
#define KEEPALIVE_COUNT          3
#define FLOW_WINDOW_PACKET_LIMIT 5
#define IP_WAIT_TIMEOUT_MS       30000

static const char *TAG = "TCP_SERVER";

static bool wait_for_ip(esp_netif_t *netif, esp_netif_ip_info_t *ip_info)
{
    for (int waited = 0; waited <= IP_WAIT_TIMEOUT_MS; waited += 500) {
        if (esp_netif_get_ip_info(netif, ip_info) == ESP_OK && ip_info->ip.addr != 0) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    return false;
}

static void handle_client(const net_iface_t *iface, int sock, const struct sockaddr_in *src)
{
    char rx[RX_BUFFER_SIZE];
    char src_ip[32];
    char dst_ip[32] = "0.0.0.0";

    inet_ntoa_r(src->sin_addr, src_ip, sizeof(src_ip));

    ESP_LOGI(TAG, "[%s] New client connected from %s:%d", iface->name, src_ip, ntohs(src->sin_port));

    struct sockaddr_in local;
    socklen_t local_len = sizeof(local);
    if (getsockname(sock, (struct sockaddr *)&local, &local_len) == 0) {
        inet_ntoa_r(local.sin_addr, dst_ip, sizeof(dst_ip));
    }

    flow_features_t flow;
    feature_extractor_init(&flow, dst_ip, TCP_SERVER_PORT, "TCP");

    while (1) {
        int len = recv(sock, rx, sizeof(rx) - 1, 0);
        if (len < 0) {
            ESP_LOGE(TAG, "[%s] recv failed: errno %d", iface->name, errno);
            break;
        }
        if (len == 0) {
            ESP_LOGW(TAG, "[%s] Connection closed by %s", iface->name, src_ip);
            break;
        }
        rx[len] = '\0';

        feature_extractor_update(&flow, src_ip, ntohs(src->sin_port), len);
        feature_extractor_print(&flow, rx);

        if (flow.in_pkts >= FLOW_WINDOW_PACKET_LIMIT)
        {
            feature_extractor_print_ml_vector(&flow);
            feature_extractor_detect(&flow);
            feature_extractor_reset_window(&flow);
        }
    }
}

static void tcp_server_task(void *pvParameters)
{
    net_iface_t *iface = (net_iface_t *)pvParameters;   /* heap copy, owned by this task */
    char local_ip[32];

    esp_netif_ip_info_t ip_info;
    if (!wait_for_ip(iface->netif, &ip_info)) {
        ESP_LOGE(TAG, "[%s] interface never got an IP address, TCP server not started", iface->name);
        free(iface);
        vTaskDelete(NULL);
        return;
    }
    esp_ip4addr_ntoa(&ip_info.ip, local_ip, sizeof(local_ip));

    struct sockaddr_in dest = {0};
    dest.sin_family      = AF_INET;
    dest.sin_addr.s_addr = ip_info.ip.addr;              /* bind to THIS interface only */
    dest.sin_port        = htons(TCP_SERVER_PORT);

    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "[%s] Unable to create socket: errno %d", iface->name, errno);
        free(iface);
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(listen_sock, (struct sockaddr *)&dest, sizeof(dest)) != 0) {
        ESP_LOGE(TAG, "[%s] Bind to %s:%d failed: errno %d", iface->name, local_ip, TCP_SERVER_PORT, errno);
        goto CLEAN_UP;
    }
    if (listen(listen_sock, 1) != 0) {
        ESP_LOGE(TAG, "[%s] Listen failed: errno %d", iface->name, errno);
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "[%s] TCP server listening on %s:%d", iface->name, local_ip, TCP_SERVER_PORT);

    while (1) {
        struct sockaddr_in src;
        socklen_t src_len = sizeof(src);
        int sock = accept(listen_sock, (struct sockaddr *)&src, &src_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "[%s] accept failed: errno %d", iface->name, errno);
            break;
        }

        int ka = 1, idle = KEEPALIVE_IDLE, intvl = KEEPALIVE_INTERVAL, cnt = KEEPALIVE_COUNT;
        setsockopt(sock, SOL_SOCKET,  SO_KEEPALIVE,  &ka,    sizeof(ka));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE,  &idle,  sizeof(idle));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT,   &cnt,   sizeof(cnt));

        handle_client(iface, sock, &src);

        shutdown(sock, SHUT_RDWR);
        close(sock);
    }

CLEAN_UP:
    close(listen_sock);
    free(iface);
    vTaskDelete(NULL);
}

void tcp_server_start(const net_iface_t *iface)
{
    net_iface_t *copy = malloc(sizeof(net_iface_t));
    if (copy == NULL || iface == NULL || iface->netif == NULL) {
        ESP_LOGE(TAG, "tcp_server_start: invalid interface");
        free(copy);
        return;
    }
    *copy = *iface;

    char task_name[16];
    snprintf(task_name, sizeof(task_name), "tcp_%s", copy->name);

    ESP_LOGI(TAG, "Starting TCP server for interface %s...", copy->name);

    if (xTaskCreate(tcp_server_task, task_name, 4096, copy, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create TCP task for %s", copy->name);
        free(copy);
    }
}
