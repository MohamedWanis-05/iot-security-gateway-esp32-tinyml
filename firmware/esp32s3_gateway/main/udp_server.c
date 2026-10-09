#include "udp_server.h"

#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "feature_extractor.h"

#define UDP_SERVER_PORT 5005
#define RX_BUFFER_SIZE 128
#define GATEWAY_IP "192.168.1.14"
#define FLOW_WINDOW_PACKET_LIMIT 5

static const char *TAG = "UDP_SERVER";

static void udp_server_task(void *pvParameters)
{
    char rx_buffer[RX_BUFFER_SIZE];

    struct sockaddr_in server_addr;
    struct sockaddr_in source_addr;
    socklen_t source_addr_len = sizeof(source_addr);

    flow_features_t flow_features;

    feature_extractor_init(
        &flow_features,
        GATEWAY_IP,
        UDP_SERVER_PORT,
        "UDP"
    );

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "UDP socket created");

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(UDP_SERVER_PORT);

    int err = bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));

    if (err < 0) {
        ESP_LOGE(TAG, "Socket unable to bind");
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "UDP server listening on port %d", UDP_SERVER_PORT);

    while (1) {
        int len = recvfrom(
            sock,
            rx_buffer,
            sizeof(rx_buffer) - 1,
            0,
            (struct sockaddr *)&source_addr,
            &source_addr_len
        );

        if (len < 0) {
            ESP_LOGE(TAG, "recvfrom failed");
            continue;
        }

        rx_buffer[len] = '\0';

        char source_ip[32];
        inet_ntoa_r(source_addr.sin_addr, source_ip, sizeof(source_ip));

        int source_port = ntohs(source_addr.sin_port);

        feature_extractor_update(
            &flow_features,
            source_ip,
            source_port,
            len
);

        feature_extractor_print(&flow_features, rx_buffer);

        if (flow_features.in_pkts >= FLOW_WINDOW_PACKET_LIMIT) {
            ESP_LOGI(TAG, "Flow window completed after %d packets", FLOW_WINDOW_PACKET_LIMIT);

            feature_extractor_print_ml_vector(&flow_features);
            feature_extractor_detect(&flow_features);

            ESP_LOGI(TAG, "Resetting flow window...");
            feature_extractor_reset_window(&flow_features);
                }
        }

    close(sock);
    vTaskDelete(NULL);
}

void udp_server_start(void)
{
    ESP_LOGI(TAG, "Starting UDP server...");

    xTaskCreate(
        udp_server_task,
        "udp_server_task",
        4096,
        NULL,
        5,
        NULL
    );
}