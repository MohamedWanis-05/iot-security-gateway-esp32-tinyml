# ESP32-S3 Gateway Firmware

This ESP-IDF firmware runs on the ESP32-S3 board and acts as the WiFi-based IoT Gateway.

## Functionality

- Connects to WiFi
- Starts a UDP server on port 5005
- Receives UDP packets from the ESP32 IoT Device
- Logs traffic metadata:
  - Source IP
  - Source Port
  - Destination Port
  - Protocol
  - Packet Length
  - Packet Count
  - Total Bytes
  - Duration
  - Payload

## Build and Flash

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p COM5 flash monitor
```

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
