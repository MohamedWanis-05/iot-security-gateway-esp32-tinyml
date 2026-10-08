/*#include "w5500_manager.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_eth.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "esp_eth_mac_w5500.h"
#include "esp_eth_phy_w5500.h"

static const char *TAG = "W5500_MANAGER";

#define ETH_CONNECTED_BIT BIT0
#define ETH_FAIL_BIT      BIT1

// عدّل الـ pins حسب التوصيل الحقيقي
#define ETH_SPI_HOST       SPI2_HOST
#define ETH_SPI_MISO_GPIO  13
#define ETH_SPI_MOSI_GPIO  11
#define ETH_SPI_SCLK_GPIO  12
#define ETH_SPI_CS_GPIO    10
#define ETH_SPI_INT_GPIO   9
#define ETH_SPI_RST_GPIO   14

#define ETH_SPI_CLOCK_MHZ  20

static EventGroupHandle_t s_eth_event_group;

static void eth_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
)
{
    uint8_t mac_addr[6] = {0};
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
        case ETHERNET_EVENT_CONNECTED:
            esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
            ESP_LOGI(TAG, "Ethernet Link Up");
            ESP_LOGI(
                TAG,
                "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                mac_addr[0],
                mac_addr[1],
                mac_addr[2],
                mac_addr[3],
                mac_addr[4],
                mac_addr[5]
            );
            break;

        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Ethernet Link Down");
            break;

        case ETHERNET_EVENT_START:
            ESP_LOGI(TAG, "Ethernet Started");
            break;

        case ETHERNET_EVENT_STOP:
            ESP_LOGI(TAG, "Ethernet Stopped");
            break;

        default:
            break;
    }
}

static void got_ip_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP");
    ESP_LOGI(TAG, "IP Address: " IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "Netmask   : " IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "Gateway   : " IPSTR, IP2STR(&ip_info->gw));

    xEventGroupSetBits(s_eth_event_group, ETH_CONNECTED_BIT);
}

void w5500_manager_start(void)
{
    ESP_LOGI(TAG, "Starting W5500 Ethernet...");

    s_eth_event_group = xEventGroupCreate();

    // لو WiFi manager already عمل esp_netif_init/event loop، ممكن دول يرجعوا invalid state
    esp_err_t ret;

    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(ret);
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(ret);
    }

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);
    ESP_ERROR_CHECK(eth_netif == NULL ? ESP_FAIL : ESP_OK);

    // 1) Init SPI bus
    spi_bus_config_t buscfg = {
        .miso_io_num = ETH_SPI_MISO_GPIO,
        .mosi_io_num = ETH_SPI_MOSI_GPIO,
        .sclk_io_num = ETH_SPI_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    ESP_ERROR_CHECK(
        spi_bus_initialize(
            ETH_SPI_HOST,
            &buscfg,
            SPI_DMA_CH_AUTO
        )
    );

    // 2) SPI device config for W5500
    spi_device_interface_config_t spi_devcfg = {
        .mode = 0,
        .clock_speed_hz = ETH_SPI_CLOCK_MHZ * 1000 * 1000,
        .queue_size = 20,
        .spics_io_num = ETH_SPI_CS_GPIO,
    };

    // 3) MAC + PHY config
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    phy_config.phy_addr = 1;
    phy_config.reset_gpio_num = ETH_SPI_RST_GPIO;

    eth_w5500_config_t w5500_config =
        ETH_W5500_DEFAULT_CONFIG(ETH_SPI_HOST, &spi_devcfg);

    w5500_config.base.int_gpio_num = ETH_SPI_INT_GPIO;

    esp_eth_mac_t *mac =
        esp_eth_mac_new_w5500(&w5500_config, &mac_config);

    esp_eth_phy_t *phy =
        esp_eth_phy_new_w5500(&phy_config);

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);

    esp_eth_handle_t eth_handle = NULL;

    ESP_ERROR_CHECK(
        esp_eth_driver_install(
            &eth_config,
            &eth_handle
        )
    );

    // بعض W5500 modules مفيهاش factory MAC، فنحدد MAC يدوي
    uint8_t eth_mac_addr[6] = {0x02, 0x00, 0x00, 0x12, 0x34, 0x56};
    ESP_ERROR_CHECK(
        esp_eth_ioctl(
            eth_handle,
            ETH_CMD_S_MAC_ADDR,
            eth_mac_addr
        )
    );

    ESP_ERROR_CHECK(
        esp_netif_attach(
            eth_netif,
            esp_eth_new_netif_glue(eth_handle)
        )
    );

    ESP_ERROR_CHECK(
        esp_event_handler_register(
            ETH_EVENT,
            ESP_EVENT_ANY_ID,
            &eth_event_handler,
            NULL
        )
    );

    ESP_ERROR_CHECK(
        esp_event_handler_register(
            IP_EVENT,
            IP_EVENT_ETH_GOT_IP,
            &got_ip_event_handler,
            NULL
        )
    );

    ESP_ERROR_CHECK(esp_eth_start(eth_handle));

    ESP_LOGI(TAG, "Waiting for Ethernet IP...");

    EventBits_t bits = xEventGroupWaitBits(
        s_eth_event_group,
        ETH_CONNECTED_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(15000)
    );

    if (bits & ETH_CONNECTED_BIT) {
        ESP_LOGI(TAG, "W5500 Ethernet is ready");
    } else {
        ESP_LOGW(TAG, "Ethernet did not get IP within timeout");
    }
}*/