# ESP32-S3 Gateway Firmware

This ESP-IDF firmware runs the IoT gateway on an ESP32-S3 with Wi-Fi and
W5500 Ethernet.

## Functionality

- Starts UDP port 5005 and TCP port 5006 on the available interfaces.
- Logs socket-level statistics and the Week 2 baseline through the local servers.
- Captures IPv4 TCP and UDP packets at the W5500 interface for bidirectional
  flow extraction.

## Build and Flash

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with the board's serial port, such as `COM5` on Windows.
See the [W5500 pin definitions](main/eth_manager.c) for the Ethernet wiring.

## Ethernet flow extraction

The W5500 capture path observes IPv4 TCP and UDP in both directions. Look for
`FLOW_COLUMNS` in the serial monitor for the column order and `FLOW_CSV` for
completed flows. A row is emitted after 30 seconds of inactivity, at the
120-second active timeout, on TCP reset, or when the 32-entry table fills.

To check both directions, connect a wired host to the gateway and open a TCP
connection to `192.168.50.1:5006`. Send a short message, close the connection,
and check for nonzero packet counts in both directions after the idle timeout.

The byte counters and packet size extrema use IPv4 Total Length. Rates use the
whole observed flow duration, with zero for a zero-duration flow. These are
firmware definitions. Compare the output against the NF-ToN-IoT-v2 exporter
on the same capture before using it with a trained model.
