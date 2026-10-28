#include "eth_manager.h"

#include <string.h>

#include "esp_log.h"
#include "esp_err.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

/* From the managed component espressif/w5500 (see main/idf_component.yml) */
#include "esp_eth_mac_w5500.h"
#include "esp_eth_phy_w5500.h"

/* ------------------------------------------------------------------ */
/*  Wiring (edit to match your board). Avoid GPIO 26-37 (flash/PSRAM), */
/*  19/20 (USB) and the strapping pins 0, 3, 45, 46.                   */
/* ------------------------------------------------------------------ */
#define ETH_SPI_HOST        SPI2_HOST
#define ETH_SPI_CLOCK_MHZ   20          /* raise to 36 once wiring is proven */
#define ETH_PIN_SCLK        12
#define ETH_PIN_MOSI        11
#define ETH_PIN_MISO        13
#define ETH_PIN_CS          10
#define ETH_PIN_INT         14          /* set to -1 if your module has no INT pin (polling mode) */
#define ETH_PIN_RST         9           /* set to -1 if RST is not wired */
#define ETH_POLL_PERIOD_MS  10          /* only used when ETH_PIN_INT < 0 */

/* ------------------------------------------------------------------ */
/*  IP configuration of the Ethernet interface                         */
/*  Must be a DIFFERENT subnet from the Wi-Fi network.                 */
/* ------------------------------------------------------------------ */
#define ETH_USE_DHCP_CLIENT     0       /* 1 = get the address from the network on the cable   */
#define ETH_ENABLE_DHCP_SERVER  1       /* 1 = hand out addresses to devices on the cable      */
#define ETH_IP_ADDR             "192.168.50.1"
#define ETH_NETMASK             "255.255.255.0"

static const char *TAG = "ETH_MANAGER";

static esp_netif_t       *s_eth_netif   = NULL;
static esp_eth_handle_t   s_eth_handle  = NULL;

static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case ETHERNET_EVENT_CONNECTED:
    {
        uint8_t mac[6] = {0};
        esp_eth_handle_t handle = *(esp_eth_handle_t *)event_data;
        esp_eth_ioctl(handle, ETH_CMD_G_MAC_ADDR, mac);
        ESP_LOGI(TAG, "Ethernet link UP, MAC %02x:%02x:%02x:%02x:%02x:%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        break;
    }
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Ethernet link DOWN (cable unplugged?)");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet driver started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet driver stopped");
        break;
    default:
        break;
    }
}

static void eth_got_ip_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Ethernet got IP: " IPSTR "  mask: " IPSTR "  gw: " IPSTR,
             IP2STR(&event->ip_info.ip),
             IP2STR(&event->ip_info.netmask),
             IP2STR(&event->ip_info.gw));
}

esp_netif_t *eth_manager_get_netif(void)
{
    return s_eth_netif;
}

bool eth_manager_start(void)
{
    esp_err_t err;

    /* ---- Shared system services (safe to call even if Wi-Fi already did) ---- */
    ESP_ERROR_CHECK(esp_netif_init());

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        ESP_ERROR_CHECK(err);
    }

    /* The W5500 INT pin is serviced through the GPIO ISR service */
    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "gpio_install_isr_service failed: %s", esp_err_to_name(err));
        return false;
    }

    /* ---- SPI bus ---- */
    spi_bus_config_t buscfg = {
        .miso_io_num   = ETH_PIN_MISO,
        .mosi_io_num   = ETH_PIN_MOSI,
        .sclk_io_num   = ETH_PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    err = spi_bus_initialize(ETH_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(err));
        return false;
    }

    /* ---- W5500 MAC + PHY drivers ---- */
    spi_device_interface_config_t devcfg = {
        .mode           = 0,
        .clock_speed_hz = ETH_SPI_CLOCK_MHZ * 1000 * 1000,
        .spics_io_num   = ETH_PIN_CS,
        .queue_size     = 20,
    };

    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(ETH_SPI_HOST, &devcfg);
#if ETH_PIN_INT >= 0
    w5500_config.base.int_gpio_num = ETH_PIN_INT;
#else
    w5500_config.base.int_gpio_num   = -1;
    w5500_config.base.poll_period_ms = ETH_POLL_PERIOD_MS;
#endif

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    mac_config.rx_task_stack_size = 4096;

    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr        = 1;
    phy_config.reset_gpio_num  = ETH_PIN_RST;

    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);
    if (mac == NULL || phy == NULL)
    {
        ESP_LOGE(TAG, "Failed to create W5500 MAC/PHY (check wiring and 3.3V supply)");
        return false;
    }

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    err = esp_eth_driver_install(&eth_config, &s_eth_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_eth_driver_install failed: %s (no W5500 answering on SPI?)",
                 esp_err_to_name(err));
        return false;
    }

    /* Give the module a unique MAC derived from the chip's base MAC.
     * (Some W5500 modules ship with identical or empty MAC addresses.) */
    uint8_t mac_addr[6] = {0};
    if (esp_read_mac(mac_addr, ESP_MAC_ETH) == ESP_OK)
    {
        esp_eth_ioctl(s_eth_handle, ETH_CMD_S_MAC_ADDR, mac_addr);
    }

    /* ---- esp_netif (the lwIP interface that owns the IP address) ---- */
    esp_netif_inherent_config_t base_cfg = ESP_NETIF_INHERENT_DEFAULT_ETH();
    esp_netif_ip_info_t static_ip;
    memset(&static_ip, 0, sizeof(static_ip));

#if !ETH_USE_DHCP_CLIENT
    esp_netif_str_to_ip4(ETH_IP_ADDR, &static_ip.ip);
    esp_netif_str_to_ip4(ETH_NETMASK, &static_ip.netmask);
    static_ip.gw = static_ip.ip;                 /* we are the gateway of this LAN */

    base_cfg.ip_info = &static_ip;
#if ETH_ENABLE_DHCP_SERVER
    base_cfg.flags = (esp_netif_flags_t)(ESP_NETIF_DHCP_SERVER | ESP_NETIF_FLAG_AUTOUP);
#else
    base_cfg.flags = (esp_netif_flags_t)(ESP_NETIF_FLAG_AUTOUP);
#endif
#endif
    base_cfg.if_key     = "ETH_W5500";
    base_cfg.if_desc    = "w5500";
    base_cfg.route_prio = 30;                    /* lower than Wi-Fi STA (100): Wi-Fi stays the default route */

    esp_netif_config_t netif_cfg = {
        .base   = &base_cfg,
        .driver = NULL,
        .stack  = ESP_NETIF_NETSTACK_DEFAULT_ETH,
    };

    s_eth_netif = esp_netif_new(&netif_cfg);
    if (s_eth_netif == NULL)
    {
        ESP_LOGE(TAG, "esp_netif_new failed");
        return false;
    }

    /* Glue links the driver to the netif and wires the default link up/down actions */
    ESP_ERROR_CHECK(esp_netif_attach(s_eth_netif, esp_eth_new_netif_glue(s_eth_handle)));

    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID,
                                               &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP,
                                               &eth_got_ip_handler, NULL));

    ESP_ERROR_CHECK(esp_eth_start(s_eth_handle));

#if !ETH_USE_DHCP_CLIENT
    ESP_LOGI(TAG, "W5500 started, static IP " ETH_IP_ADDR "/" ETH_NETMASK "%s",
             ETH_ENABLE_DHCP_SERVER ? " (DHCP server on)" : "");
#else
    ESP_LOGI(TAG, "W5500 started, waiting for DHCP lease...");
#endif
    return true;
}
