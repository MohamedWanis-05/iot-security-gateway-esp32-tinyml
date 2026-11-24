# IoT Security Gateway Using ESP32-S3 and TinyML

This graduation project uses an ESP32-S3 to observe IoT network traffic and turn
it into flow features. The goal is to use those features for lightweight
multiclass intrusion detection on the gateway.

## Project status

| Stage | Status |
| --- | --- |
| Week 1 · UDP communication | An ESP32 test device sends UDP messages to the gateway over Wi-Fi. |
| Week 2 · Feature extraction prototype | The UDP server groups five received datagrams, logs a nine-value vector, and runs a threshold-based baseline. |
| Ethernet and TCP support | The gateway starts local UDP and TCP servers on ports 5005 and 5006 on the available Wi-Fi and W5500 Ethernet interfaces. |
| Bidirectional flow extraction | The W5500 capture path combines IPv4 TCP and UDP packets in both directions into one flow and exports 18 values. The firmware build and host tests passed. On-board validation is pending. |
| Multiclass TinyML | Compare the firmware output with the NetFlow exporter, select training inputs, then train and deploy the model. |

## Repository layout

- [ESP32-S3 gateway firmware](firmware/esp32s3_gateway/README.md) handles network
  interfaces, local servers, packet capture, and flow aggregation.
- [ESP32 test device firmware](firmware/esp32_iot_device/README.md) generates
  sample UDP traffic.

## Try Ethernet flow extraction

1. Build and flash the ESP32-S3 using the [gateway setup instructions](firmware/esp32s3_gateway/README.md#build-and-flash).
2. Connect a host to the W5500 Ethernet network. Open a TCP connection to
   `192.168.50.1:5006`, send a short message, and close the connection.
3. In the serial monitor, look for `FLOW_COLUMNS` and then `FLOW_CSV`. After
   30 seconds of inactivity, check that the TCP row has nonzero `IN_PKTS` and
   `OUT_PKTS`.

The Ethernet output includes ports, IP protocol, packet and byte counts in
both directions, flow duration, packet size extrema, client TCP flags, the
maximum forward TCP window, and directional rates in bytes/s and bits/s. The
gateway pairs packets by protocol and endpoint IP addresses and ports. The
addresses identify flows internally.

## Dataset and model

The feature names follow the [NF-ToN-IoT-v2 dataset](https://www.cyber.uq.edu.au/project/machine-learning-based-nids-datasets).
`Attack` is the multiclass training target. The current firmware measures
`IPv4 Total Length` and divides each direction's byte count by the whole
observed flow duration. Before training, compare these values with the
reference exporter on the same packet capture and fix the model input order
and preprocessing. See the [gateway extraction notes](firmware/esp32s3_gateway/README.md#ethernet-flow-extraction).
