#include "udp_server.h"

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_timer.h"

#define UDP_SERVER_PORT 5005
#define RX_BUFFER_SIZE 128

static const char *TAG = "UDP_SERVER";

static void udp_server_task(void *pvParameters)
{
    char rx_buffer[RX_BUFFER_SIZE];

    struct sockaddr_in server_addr;
    struct sockaddr_in source_addr;
    socklen_t source_addr_len = sizeof(source_addr);

    int packet_count = 0;
    int total_bytes = 0;

    int64_t first_packet_time_ms = 0;
    int64_t last_packet_time_ms = 0;

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

        packet_count++;
        total_bytes += len;

        int64_t now_ms = esp_timer_get_time() / 1000;

        if (packet_count == 1) {
            first_packet_time_ms = now_ms;
        }

        last_packet_time_ms = now_ms;

        int duration_ms = (int)(last_packet_time_ms - first_packet_time_ms);

        char source_ip[32];
        inet_ntoa_r(source_addr.sin_addr, source_ip, sizeof(source_ip));

        int source_port = ntohs(source_addr.sin_port);

        ESP_LOGI(TAG, "----- UDP Traffic Metadata -----");
        ESP_LOGI(TAG, "SRC_IP      : %s", source_ip);
        ESP_LOGI(TAG, "SRC_PORT    : %d", source_port);
        ESP_LOGI(TAG, "DST_PORT    : %d", UDP_SERVER_PORT);
        ESP_LOGI(TAG, "PROTOCOL    : UDP");
        ESP_LOGI(TAG, "PACKET_LEN  : %d bytes", len);
        ESP_LOGI(TAG, "PACKETS     : %d", packet_count);
        ESP_LOGI(TAG, "TOTAL_BYTES : %d", total_bytes);
        ESP_LOGI(TAG, "DURATION_MS : %d", duration_ms);
        ESP_LOGI(TAG, "PAYLOAD     : %s", rx_buffer);
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