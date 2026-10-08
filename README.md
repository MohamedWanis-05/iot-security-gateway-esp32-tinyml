# IoT Security Gateway Using ESP32-S3 and TinyML

## Overview

This graduation project aims to build a lightweight IoT Security Gateway using ESP32-S3 and TinyML. The system monitors IoT traffic, extracts network metadata/features, and performs lightweight intrusion or anomaly detection on embedded hardware.

## Current Status

### Week 1 — WiFi UDP Communication Prototype

Completed:

- ESP32-S3 configured as Gateway.
- ESP32 configured as IoT Device.
- Both devices connected to the same WiFi network.
- Gateway runs a UDP server on port 5005.
- IoT Device sends UDP packets every 5 seconds.
- Gateway receives packets and logs traffic metadata.

Observed metadata:

```text
SRC_IP      : 192.168.1.15
DST_PORT    : 5005
PROTOCOL    : UDP
PACKET_LEN  : 54 bytes
PAYLOAD     : iot_device=esp32;temperature=25;humidity=60;packet=...
