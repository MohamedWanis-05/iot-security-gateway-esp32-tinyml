#include "traffic_logger.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "TRAFFIC_LOGGER";

typedef struct {
    const char *src_ip;
    const char *dst_ip;
    int src_port;
    int dst_port;
    const char *protocol;
    int packets;
    int bytes;
    int duration_ms;
} traffic_metadata_t;

static void print_metadata(const traffic_metadata_t *m)
{
    ESP_LOGI(TAG, "----- Traffic Metadata -----");
    ESP_LOGI(TAG, "SRC_IP      : %s", m->src_ip);
    ESP_LOGI(TAG, "DST_IP      : %s", m->dst_ip);
    ESP_LOGI(TAG, "SRC_PORT    : %d", m->src_port);
    ESP_LOGI(TAG, "DST_PORT    : %d", m->dst_port);
    ESP_LOGI(TAG, "PROTOCOL    : %s", m->protocol);
    ESP_LOGI(TAG, "PACKETS     : %d", m->packets);
    ESP_LOGI(TAG, "BYTES       : %d", m->bytes);
    ESP_LOGI(TAG, "DURATION_MS : %d", m->duration_ms);
}

static void traffic_logger_task(void *pvParameters)
{
    while (1) {
        traffic_metadata_t sample = {
            .src_ip = "192.168.1.20",
            .dst_ip = "192.168.1.10",
            .src_port = 52344,
            .dst_port = 80,
            .protocol = "TCP",
            .packets = 12,
            .bytes = 2300,
            .duration_ms = 150
        };

        print_metadata(&sample);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void traffic_logger_start(void)
{
    ESP_LOGI(TAG, "Starting traffic metadata logger...");

    xTaskCreate(
        traffic_logger_task,
        "traffic_logger_task",
        4096,
        NULL,
        5,
        NULL
    );
}