# ESP32 IoT Device Firmware

This ESP-IDF firmware runs on a second ESP32 board and acts as a test IoT device.

## Functionality

- Connects to the same WiFi network as the Gateway
- Sends UDP packets to the ESP32-S3 Gateway every 5 seconds
- Sends sample sensor-like payloads containing temperature, humidity, and packet counter

## Gateway Configuration

```text
Gateway IP: 192.168.1.14
Gateway Port: 5005
Protocol: UDP