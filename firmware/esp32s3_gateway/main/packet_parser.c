#include "flow_table.h"

static uint16_t read_be16(const uint8_t *bytes)
{
    return (uint16_t)((bytes[0] << 8) | bytes[1]);
}

static uint32_t read_be32(const uint8_t *bytes)
{
    return ((uint32_t)read_be16(bytes) << 16) | read_be16(bytes + 2);
}

bool packet_parse(const uint8_t *frame, size_t length, packet_metadata_t *packet)
{
    if (length < 14) {
        return false;
    }

    size_t offset = 14;
    uint16_t type = read_be16(frame + 12);
    while (type == 0x8100 || type == 0x88a8) {
        if (length - offset < 4) {
            return false;
        }
        type = read_be16(frame + offset + 2);
        offset += 4;
    }
    if (type != 0x0800 || length - offset < 20) {
        return false;
    }

    const uint8_t *ip = frame + offset;
    size_t ip_header = (ip[0] & 0x0f) * 4;
    uint16_t total = read_be16(ip + 2);
    if ((ip[0] >> 4) != 4 || ip_header < 20 || total < ip_header ||
        total > length - offset || (read_be16(ip + 6) & 0x3fff) != 0) {
        return false;
    }

    packet_metadata_t parsed = {
        .key = { .src_ip = read_be32(ip + 12), .dst_ip = read_be32(ip + 16),
                 .protocol = ip[9] },
        .ip_length = total,
    };
    const uint8_t *transport = ip + ip_header;
    size_t transport_length = total - ip_header;
    switch (parsed.key.protocol) {
    case 6:
        if (transport_length < 20 || (transport[12] >> 4) < 5 ||
            (size_t)(transport[12] >> 4) * 4 > transport_length) {
            return false;
        }
        parsed.tcp_flags = transport[13];
        parsed.tcp_window = read_be16(transport + 14);
        break;
    case 17:
        if (transport_length < 8 || read_be16(transport + 4) < 8 ||
            read_be16(transport + 4) > transport_length) {
            return false;
        }
        break;
    default:
        return false;
    }
    parsed.key.src_port = read_be16(transport);
    parsed.key.dst_port = read_be16(transport + 2);
    *packet = parsed;
    return true;
}
