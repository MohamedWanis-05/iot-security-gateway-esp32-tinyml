#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FLOW_CAPACITY 32
#define FLOW_IDLE_MS 30000
#define FLOW_ACTIVE_MS 120000

typedef struct {
    uint32_t src_ip, dst_ip;
    uint16_t src_port, dst_port;
    uint8_t protocol;
} flow_key_t;

typedef struct {
    flow_key_t key;
    uint64_t time_ms;
    uint16_t ip_length, tcp_window;
    uint8_t tcp_flags;
} packet_metadata_t;

typedef struct {
    uint64_t bytes, packets;
    uint16_t max_window;
    uint8_t tcp_flags;
} flow_direction_t;

typedef struct {
    bool used;
    flow_key_t key;
    uint64_t first_ms, last_ms;
    uint16_t min_ip, max_ip;
    flow_direction_t forward, reverse;
} packet_flow_t;

typedef struct {
    packet_flow_t entries[FLOW_CAPACITY];
} flow_table_t;

typedef void (*flow_emit_fn)(const packet_flow_t *flow);

bool packet_parse(const uint8_t *frame, size_t length, packet_metadata_t *packet);
void flow_table_expire(flow_table_t *table, uint64_t now_ms, flow_emit_fn emit);
void flow_table_update(flow_table_t *table, const packet_metadata_t *packet, flow_emit_fn emit);
