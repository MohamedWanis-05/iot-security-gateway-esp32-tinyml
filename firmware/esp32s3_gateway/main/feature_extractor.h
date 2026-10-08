#ifndef FEATURE_EXTRACTOR_H
#define FEATURE_EXTRACTOR_H

#include <stdint.h>

#define FEATURE_IP_STRING_SIZE 32
#define FEATURE_PROTOCOL_STRING_SIZE 8

typedef struct {
    char src_ip[FEATURE_IP_STRING_SIZE];
    char dst_ip[FEATURE_IP_STRING_SIZE];

    int l4_src_port;
    int l4_dst_port;

    char protocol[FEATURE_PROTOCOL_STRING_SIZE];

    int in_pkts;
    int in_bytes;

    int longest_flow_pkt;
    int shortest_flow_pkt;

    int flow_duration_ms;

    int64_t first_packet_time_ms;
    int64_t last_packet_time_ms;
} flow_features_t;

void feature_extractor_init(
    flow_features_t *features,
    const char *dst_ip,
    int dst_port,
    const char *protocol
);

void feature_extractor_update(
    flow_features_t *features,
    const char *src_ip,
    int src_port,
    int packet_length
);

void feature_extractor_print(
    const flow_features_t *features,
    const char *payload
);

#endif