#include "packet_capture.h"

#include <inttypes.h>
#include <stdatomic.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#if CONFIG_ESP_NETIF_L2_TAP
#include "esp_vfs_l2tap.h"
#endif

#include "flow_table.h"

static const char *TAG = "NETFLOW";
static QueueHandle_t s_packets;
static flow_table_t s_flows;
static atomic_uint s_dropped;
static esp_err_t (*s_transmit)(esp_eth_mac_t *, uint8_t *, uint32_t);

static uint64_t time_ms(void)
{
    return (uint64_t)esp_timer_get_time() / 1000;
}

static void emit_flow(const packet_flow_t *flow)
{
    uint64_t duration_ms = flow->last_ms - flow->first_ms;
    double seconds = duration_ms / 1000.0;
    double forward_rate = duration_ms ? flow->forward.bytes / seconds : 0.0;
    double reverse_rate = duration_ms ? flow->reverse.bytes / seconds : 0.0;

    ESP_LOGI(TAG, "FLOW_CSV,%u,%u,%u,%" PRIu64 ",%" PRIu64 ",%" PRIu64
             ",%" PRIu64 ",%" PRIu64 ",%u,%u,%u,%u,%u,%u,%.6f,%.6f,%.6f,%.6f",
             (unsigned)flow->key.src_port, (unsigned)flow->key.dst_port,
             (unsigned)flow->key.protocol,
             flow->forward.bytes, flow->forward.packets,
             flow->reverse.bytes, flow->reverse.packets, duration_ms,
             (unsigned)flow->forward.tcp_flags,
             (unsigned)flow->max_ip, (unsigned)flow->min_ip,
             (unsigned)flow->min_ip, (unsigned)flow->max_ip,
             (unsigned)flow->forward.max_window,
             forward_rate, reverse_rate, 8.0 * forward_rate, 8.0 * reverse_rate);
}

static void enqueue(const packet_metadata_t *packet)
{
    if (xQueueSend(s_packets, packet, 0) != pdPASS) {
        atomic_fetch_add_explicit(&s_dropped, 1, memory_order_relaxed);
    }
}

static esp_err_t capture_rx(esp_eth_handle_t handle, uint8_t *buffer, uint32_t length,
                            void *context, void *info)
{
    packet_metadata_t packet;
    if (packet_parse(buffer, length, &packet)) {
        packet.time_ms = time_ms();
        enqueue(&packet);
    }
#if CONFIG_ESP_NETIF_L2_TAP
    size_t remaining = length;
    esp_err_t result = esp_vfs_l2tap_eth_filter_frame(handle, buffer, &remaining, info);
    if (remaining == 0) {
        return result;
    }
    length = (uint32_t)remaining;
#else
    (void)handle;
    (void)info;
#endif
    return esp_netif_receive(context, buffer, length, NULL);
}

static esp_err_t capture_tx(esp_eth_mac_t *mac, uint8_t *buffer, uint32_t length)
{
    packet_metadata_t packet;
    bool parsed = packet_parse(buffer, length, &packet);
    packet.time_ms = time_ms();
    esp_err_t result = s_transmit(mac, buffer, length);
    if (result == ESP_OK && parsed) {
        enqueue(&packet);
    }
    return result;
}

static void capture_task(void *context)
{
    (void)context;
    ESP_LOGI(TAG, "FLOW_COLUMNS,L4_SRC_PORT,L4_DST_PORT,PROTOCOL,IN_BYTES,IN_PKTS,"
             "OUT_BYTES,OUT_PKTS,FLOW_DURATION_MILLISECONDS,CLIENT_TCP_FLAGS,"
             "LONGEST_FLOW_PKT,SHORTEST_FLOW_PKT,MIN_IP_PKT_LEN,MAX_IP_PKT_LEN,"
             "TCP_WIN_MAX_IN,SRC_TO_DST_SECOND_BYTES,DST_TO_SRC_SECOND_BYTES,"
             "SRC_TO_DST_AVG_THROUGHPUT,DST_TO_SRC_AVG_THROUGHPUT");
    uint64_t last_check = time_ms();
    while (true) {
        packet_metadata_t packet;
        if (xQueueReceive(s_packets, &packet, pdMS_TO_TICKS(250)) == pdPASS) {
            flow_table_update(&s_flows, &packet, emit_flow);
        } else {
            flow_table_expire(&s_flows, time_ms(), emit_flow);
        }
        uint64_t now_ms = time_ms();
        if (now_ms - last_check >= 1000) {
            unsigned dropped = atomic_exchange_explicit(&s_dropped, 0, memory_order_relaxed);
            if (dropped != 0) {
                ESP_LOGW(TAG, "Dropped %u packet observations in the capture queue", dropped);
            }
            last_check = now_ms;
        }
    }
}

esp_err_t packet_capture_start(esp_eth_handle_t handle, esp_eth_mac_t *mac, esp_netif_t *netif)
{
    s_packets = xQueueCreate(128, sizeof(packet_metadata_t));
    if (s_packets == NULL) {
        return ESP_ERR_NO_MEM;
    }
    TaskHandle_t task;
    if (xTaskCreate(capture_task, "netflow", 4096, NULL, 5, &task) != pdPASS) {
        vQueueDelete(s_packets);
        return ESP_ERR_NO_MEM;
    }
    esp_err_t result = esp_eth_update_input_path_info(handle, capture_rx, netif);
    if (result != ESP_OK) {
        vTaskDelete(task);
        vQueueDelete(s_packets);
        return result;
    }
    s_transmit = mac->transmit;
    mac->transmit = capture_tx;
    return ESP_OK;
}
