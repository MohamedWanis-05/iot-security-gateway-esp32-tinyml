#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "wifi_manager.h"
#include "udp_server.h"

static const char *TAG = "GATEWAY_MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "============================");
    ESP_LOGI(TAG, " ESP32-S3 IoT Security Gateway");
    ESP_LOGI(TAG, " Week 2 Feature Extraction Test");
    ESP_LOGI(TAG, "============================");

    ESP_LOGI(TAG, "Starting WiFi...");
    wifi_manager_start();

    ESP_LOGI(TAG, "Starting UDP server...");
    udp_server_start();

    while (1) {
        ESP_LOGI(TAG, "Gateway main loop alive");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}