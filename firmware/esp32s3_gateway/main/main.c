#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_netif.h"

#include "net_iface.h"
#include "wifi_manager.h"
#include "eth_manager.h"
#include "udp_server.h"
#include "tcp_server.h"

static const char *TAG = "GATEWAY_MAIN";

static void log_interface(const char *name, esp_netif_t *netif)
{
    esp_netif_ip_info_t ip;

    if (netif != NULL && esp_netif_get_ip_info(netif, &ip) == ESP_OK && ip.ip.addr != 0) {
        ESP_LOGI(TAG, "  %-5s IP " IPSTR "  mask " IPSTR, name, IP2STR(&ip.ip), IP2STR(&ip.netmask));
    } else {
        ESP_LOGW(TAG, "  %-5s no IP address", name);
    }
}

static void start_interface_servers(const char *name, esp_netif_t *netif)
{
    net_iface_t iface = { .netif = netif };
    strncpy(iface.name, name, sizeof(iface.name) - 1);

    udp_server_start(&iface);
    tcp_server_start(&iface);
}

void app_main(void)
{
    ESP_LOGI(TAG, "============================");
    ESP_LOGI(TAG, " ESP32-S3 IoT Security Gateway");
    ESP_LOGI(TAG, " Dual interface: WiFi + W5500");
    ESP_LOGI(TAG, "============================");

    ESP_LOGI(TAG, "Starting WiFi...");
    bool wifi_ok = wifi_manager_start();

    ESP_LOGI(TAG, "Starting W5500 Ethernet...");
    bool eth_ok = eth_manager_start();

    if (wifi_ok) {
        start_interface_servers("WIFI", wifi_manager_get_netif());
    } else {
        ESP_LOGE(TAG, "WiFi is down - running on Ethernet only");
    }

    if (eth_ok) {
        start_interface_servers("ETH", eth_manager_get_netif());
    } else {
        ESP_LOGE(TAG, "Ethernet failed to start - running on WiFi only");
    }

    while (1) {
        ESP_LOGI(TAG, "Gateway main loop alive. Interfaces:");
        log_interface("WIFI", wifi_manager_get_netif());
        log_interface("ETH",  eth_manager_get_netif());
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
