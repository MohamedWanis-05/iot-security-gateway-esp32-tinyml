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