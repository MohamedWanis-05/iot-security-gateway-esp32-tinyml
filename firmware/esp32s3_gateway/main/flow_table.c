#include "flow_table.h"

static bool same_key(flow_key_t a, flow_key_t b)
{
    return a.src_ip == b.src_ip && a.dst_ip == b.dst_ip &&
           a.src_port == b.src_port && a.dst_port == b.dst_port &&
           a.protocol == b.protocol;
}

void flow_table_expire(flow_table_t *table, uint64_t now_ms, flow_emit_fn emit)
{
    for (size_t i = 0; i < FLOW_CAPACITY; ++i) {
        packet_flow_t *flow = &table->entries[i];
        if (flow->used && now_ms >= flow->last_ms &&
            (now_ms - flow->last_ms >= FLOW_IDLE_MS ||
             now_ms - flow->first_ms >= FLOW_ACTIVE_MS)) {
            emit(flow);
            flow->used = false;
        }
    }
}

void flow_table_update(flow_table_t *table, const packet_metadata_t *packet, flow_emit_fn emit)
{
    flow_key_t key = packet->key;
    flow_key_t reverse_key = {
        .src_ip = key.dst_ip, .dst_ip = key.src_ip,
        .src_port = key.dst_port, .dst_port = key.src_port, .protocol = key.protocol,
    };
    packet_flow_t *flow = NULL;
    packet_flow_t *available = NULL;
    packet_flow_t *oldest = &table->entries[0];
    bool reverse = false;

    flow_table_expire(table, packet->time_ms, emit);
    for (size_t i = 0; i < FLOW_CAPACITY; ++i) {
        packet_flow_t *entry = &table->entries[i];
        if (!entry->used) {
            available = entry;
            continue;
        }
        if (entry->last_ms < oldest->last_ms) {
            oldest = entry;
        }
        if (same_key(entry->key, key) || same_key(entry->key, reverse_key)) {
            flow = entry;
            reverse = !same_key(entry->key, key);
            break;
        }
    }

    if (flow == NULL) {
        flow = available != NULL ? available : oldest;
        if (flow->used) {
            emit(flow);
        }
        /* A SYN-ACK observed first still identifies the client endpoint. */
        reverse = key.protocol == 6 && (packet->tcp_flags & 0x12) == 0x12;
        *flow = (packet_flow_t){
            .used = true, .key = reverse ? reverse_key : key,
            .first_ms = packet->time_ms, .last_ms = packet->time_ms,
            .min_ip = packet->ip_length, .max_ip = packet->ip_length,
        };
    }

    flow_direction_t *direction = reverse ? &flow->reverse : &flow->forward;
    direction->packets++;
    direction->bytes += packet->ip_length;
    direction->tcp_flags |= packet->tcp_flags;
    if (packet->tcp_window > direction->max_window) {
        direction->max_window = packet->tcp_window;
    }
    if (packet->time_ms < flow->first_ms) flow->first_ms = packet->time_ms;
    if (packet->time_ms > flow->last_ms) flow->last_ms = packet->time_ms;
    if (packet->ip_length < flow->min_ip) flow->min_ip = packet->ip_length;
    if (packet->ip_length > flow->max_ip) flow->max_ip = packet->ip_length;

    if (key.protocol == 6 && (packet->tcp_flags & 0x04) != 0) {
        emit(flow);
        flow->used = false;
    }
}
