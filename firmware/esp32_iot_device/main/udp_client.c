#include "udp_client.h"

#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#define GATEWAY_IP   "192.168.1.14"
#define GATEWAY_PORT 5005

static const char *TAG = "UDP_CLIENT";

static void udp_client_task(void *pvParameters)
{
    int packet_counter = 0;

    while (1) {
        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket");
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(GATEWAY_IP);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(GATEWAY_PORT);

        char message[128];

        if (packet_counter % 3 == 0) {
            snprintf(message, sizeof(message), "small");
        } else if (packet_counter % 3 == 1) {
            snprintf(
                message,
                sizeof(message),
                "iot_device=esp32;temperature=25;humidity=60;packet=%d",
                packet_counter
            );
        } else {
            snprintf(
                message,
                sizeof(message),
                "iot_device=esp32;temperature=25;humidity=60;packet=%d;status=normal;location=test_lab;extra_payload=abcdefghijk",
                packet_counter
            );
        }

        int err = sendto(
            sock,
            message,
            strlen(message),
            0,
            (struct sockaddr *)&dest_addr,
            sizeof(dest_addr)
        );

        if (err < 0) {
            ESP_LOGE(TAG, "Error sending packet");
        } else {
            ESP_LOGI(TAG, "Sent packet to gateway: %s", message);
        }

        close(sock);

        packet_counter++;

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void udp_client_start(void)
{
    ESP_LOGI(TAG, "Starting UDP client traffic generator...");

    xTaskCreate(
        udp_client_task,
        "udp_client_task",
        4096,
        NULL,
        5,
        NULL
    );
}