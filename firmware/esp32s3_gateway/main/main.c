#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "wifi_manager.h"
#include "udp_server.h"

static const char *TAG = "IOT_DEVICE_MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "============================");
    ESP_LOGI(TAG, " ESP32 IoT Test Device");
    ESP_LOGI(TAG, " UDP Traffic Generator");
    ESP_LOGI(TAG, "============================");

    ESP_LOGI(TAG, "Connecting IoT device to WiFi...");
    wifi_manager_start();

    ESP_LOGI(TAG, "Starting UDP traffic generation...");
    udp_server_start();;

    while (1) {
        ESP_LOGI(TAG, "IoT device main loop alive");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}