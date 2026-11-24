#include <assert.h>
#include <string.h>

#include "flow_table.h"

static packet_flow_t rows[4];
static size_t row_count;

static void record_flow(const packet_flow_t *flow)
{
    assert(row_count < sizeof(rows) / sizeof(rows[0]));
    rows[row_count++] = *flow;
}

static void test_parser(void)
{
    uint8_t frame[64] = {
        [12] = 8, [14] = 0x45, [17] = 28, [23] = 17,
        [26] = 192, [29] = 2, [30] = 192, [33] = 1,
        [34] = 0x12, [35] = 0x34, [36] = 0x13, [37] = 0x8d, [39] = 8,
    };
    packet_metadata_t packet;
    assert(packet_parse(frame, 60, &packet));
    assert(packet.key.src_port == 0x1234 && packet.key.dst_port == 5005);
    assert(packet.ip_length == 28);
    assert(!packet_parse(frame, 41, &packet));
    frame[20] = 0x20;
    assert(!packet_parse(frame, 60, &packet));
    frame[20] = 0;
    frame[23] = 6;
    frame[17] = 40;
    frame[46] = 0x50;
    frame[47] = 0x12;
    frame[48] = 0x20;
    assert(packet_parse(frame, 60, &packet));
    assert(packet.tcp_flags == 0x12 && packet.tcp_window == 8192);
}

static void test_flows(void)
{
    flow_table_t table = {0};
    packet_metadata_t packet = {
        .key = { .src_ip = 2, .dst_ip = 1, .src_port = 1000, .dst_port = 5005,
                 .protocol = 17 },
        .time_ms = 10, .ip_length = 30,
    };
    flow_table_update(&table, &packet, record_flow);
    packet.key.src_ip = 3;
    flow_table_update(&table, &packet, record_flow);
    packet.key = (flow_key_t){ .src_ip = 1, .dst_ip = 2, .src_port = 5005,
                               .dst_port = 1000, .protocol = 17 };
    packet.time_ms = 35;
    packet.ip_length = 100;
    flow_table_update(&table, &packet, record_flow);
    flow_table_expire(&table, FLOW_IDLE_MS + 35, record_flow);
    assert(row_count == 2);
    const packet_flow_t *flow = rows[0].key.src_ip == 2 ? &rows[0] : &rows[1];
    assert(flow->forward.packets == 1 && flow->forward.bytes == 30);
    assert(flow->reverse.packets == 1 && flow->reverse.bytes == 100);
    assert(flow->first_ms == 10 && flow->last_ms == 35);
    assert(flow->min_ip == 30 && flow->max_ip == 100);

    packet.key.protocol = 6;
    packet.tcp_flags = 0x12;
    packet.tcp_window = 1024;
    flow_table_update(&table, &packet, record_flow);
    packet.key = (flow_key_t){ .src_ip = 2, .dst_ip = 1, .src_port = 1000,
                               .dst_port = 5005, .protocol = 6 };
    packet.tcp_flags = 0x10;
    packet.tcp_window = 2048;
    flow_table_update(&table, &packet, record_flow);
    packet.tcp_flags = 0x04;
    packet.tcp_window = 512;
    flow_table_update(&table, &packet, record_flow);
    assert(row_count == 3 && rows[2].key.src_ip == 2);
    assert(rows[2].forward.tcp_flags == 0x14 && rows[2].forward.max_window == 2048);
    assert(rows[2].reverse.tcp_flags == 0x12 && rows[2].reverse.max_window == 1024);
}

int main(void)
{
    test_parser();
    test_flows();
}
