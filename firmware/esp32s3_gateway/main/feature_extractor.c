#include "feature_extractor.h"

#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "FEATURE_EXTRACTOR";

void feature_extractor_init(
    flow_features_t *features,
    const char *dst_ip,
    int dst_port,
    const char *protocol
)
{
    memset(features, 0, sizeof(flow_features_t));

    strncpy(features->dst_ip, dst_ip, FEATURE_IP_STRING_SIZE - 1);
    features->dst_ip[FEATURE_IP_STRING_SIZE - 1] = '\0';

    strncpy(features->protocol, protocol, FEATURE_PROTOCOL_STRING_SIZE - 1);
    features->protocol[FEATURE_PROTOCOL_STRING_SIZE - 1] = '\0';

    features->l4_dst_port = dst_port;

    features->in_pkts = 0;
    features->in_bytes = 0;

    features->longest_flow_pkt = 0;
    features->shortest_flow_pkt = 0;

    features->flow_duration_ms = 0;
    features->first_packet_time_ms = 0;
    features->last_packet_time_ms = 0;
}

void feature_extractor_update(
    flow_features_t *features,
    const char *src_ip,
    int src_port,
    int packet_length
)
{
    int64_t now_ms = esp_timer_get_time() / 1000;

    if (features->in_pkts == 0) {
        features->first_packet_time_ms = now_ms;
        features->shortest_flow_pkt = packet_length;
        features->longest_flow_pkt = packet_length;
    }

    features->last_packet_time_ms = now_ms;

    strncpy(features->src_ip, src_ip, FEATURE_IP_STRING_SIZE - 1);
    features->src_ip[FEATURE_IP_STRING_SIZE - 1] = '\0';

    features->l4_src_port = src_port;

    features->in_pkts++;
    features->in_bytes += packet_length;

    if (packet_length > features->longest_flow_pkt) {
        features->longest_flow_pkt = packet_length;
    }

    if (packet_length < features->shortest_flow_pkt) {
        features->shortest_flow_pkt = packet_length;
    }

    features->flow_duration_ms =
        (int)(features->last_packet_time_ms - features->first_packet_time_ms);
}

void feature_extractor_print(
    const flow_features_t *features,
    const char *payload
)
{
    ESP_LOGI(TAG, "----- FLOW FEATURES -----");
    ESP_LOGI(TAG, "SRC_IP                     : %s", features->src_ip);
    ESP_LOGI(TAG, "DST_IP                     : %s", features->dst_ip);
    ESP_LOGI(TAG, "L4_SRC_PORT                : %d", features->l4_src_port);
    ESP_LOGI(TAG, "L4_DST_PORT                : %d", features->l4_dst_port);
    ESP_LOGI(TAG, "PROTOCOL                   : %s", features->protocol);
    ESP_LOGI(TAG, "IN_PKTS                    : %d", features->in_pkts);
    ESP_LOGI(TAG, "IN_BYTES                   : %d", features->in_bytes);
    ESP_LOGI(TAG, "LONGEST_FLOW_PKT           : %d", features->longest_flow_pkt);
    ESP_LOGI(TAG, "SHORTEST_FLOW_PKT          : %d", features->shortest_flow_pkt);
    ESP_LOGI(TAG, "FLOW_DURATION_MILLISECONDS : %d", features->flow_duration_ms);
    ESP_LOGI(TAG, "PAYLOAD                    : %s", payload);
}