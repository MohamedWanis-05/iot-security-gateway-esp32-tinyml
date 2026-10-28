#include "udp_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_netif.h"

#include "feature_extractor.h"

#define UDP_SERVER_PORT          5005
#define RX_BUFFER_SIZE           128
#define FLOW_WINDOW_PACKET_LIMIT 5
#define IP_WAIT_TIMEOUT_MS       30000   /* how long to wait for the interface to get an IP */

static const char *TAG = "UDP_SERVER";

/* Waits until the interface has a non-zero IPv4 address. */
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

static void udp_server_task(void *pvParameters)
{
    net_iface_t *iface = (net_iface_t *)pvParameters;   /* heap copy, owned by this task */
    char rx_buffer[RX_BUFFER_SIZE];
    char local_ip[32];

    struct sockaddr_in server_addr;
    struct sockaddr_in source_addr;
    socklen_t source_addr_len;

    esp_netif_ip_info_t ip_info;
    if (!wait_for_ip(iface->netif, &ip_info))
    {
        ESP_LOGE(TAG, "[%s] interface never got an IP address, UDP server not started", iface->name);
        free(iface);
        vTaskDelete(NULL);
        return;
    }
    esp_ip4addr_ntoa(&ip_info.ip, local_ip, sizeof(local_ip));

    flow_features_t flow_features;
    feature_extractor_init(&flow_features, local_ip, UDP_SERVER_PORT, "UDP");

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "[%s] Unable to create socket", iface->name);
        free(iface);
        vTaskDelete(NULL);
        return;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = ip_info.ip.addr;      /* bind to THIS interface only */
    server_addr.sin_port        = htons(UDP_SERVER_PORT);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "[%s] Socket unable to bind to %s:%d", iface->name, local_ip, UDP_SERVER_PORT);
        close(sock);
        free(iface);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "[%s] UDP server listening on %s:%d", iface->name, local_ip, UDP_SERVER_PORT);

    while (1)
    {
        source_addr_len = sizeof(source_addr);   /* must be reset before every recvfrom */

        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0,
                           (struct sockaddr *)&source_addr, &source_addr_len);
        if (len < 0)
        {
            ESP_LOGE(TAG, "[%s] recvfrom failed", iface->name);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        rx_buffer[len] = '\0';

        char source_ip[32];
        inet_ntoa_r(source_addr.sin_addr, source_ip, sizeof(source_ip));
        int source_port = ntohs(source_addr.sin_port);

        feature_extractor_update(&flow_features, source_ip, source_port, len);

        ESP_LOGI(TAG, "[%s] packet from %s:%d", iface->name, source_ip, source_port);
        feature_extractor_print(&flow_features, rx_buffer);

        if (flow_features.in_pkts >= FLOW_WINDOW_PACKET_LIMIT)
        {
            ESP_LOGI(TAG, "[%s] Flow window completed after %d packets",
                     iface->name, FLOW_WINDOW_PACKET_LIMIT);

            feature_extractor_print_ml_vector(&flow_features);
            feature_extractor_detect(&flow_features);

            ESP_LOGI(TAG, "[%s] Resetting flow window...", iface->name);
            feature_extractor_reset_window(&flow_features);
        }
    }
}

void udp_server_start(const net_iface_t *iface)
{
    net_iface_t *copy = malloc(sizeof(net_iface_t));
    if (copy == NULL || iface == NULL || iface->netif == NULL) {
        ESP_LOGE(TAG, "udp_server_start: invalid interface");
        free(copy);
        return;
    }
    *copy = *iface;

    char task_name[16];
    snprintf(task_name, sizeof(task_name), "udp_%s", copy->name);

    ESP_LOGI(TAG, "Starting UDP server for interface %s...", copy->name);

    if (xTaskCreate(udp_server_task, task_name, 4096, copy, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UDP task for %s", copy->name);
        free(copy);
    }
}
