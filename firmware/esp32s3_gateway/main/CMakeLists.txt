idf_component_register(
    SRCS
        "main.c"
        "wifi_manager.c"
        "eth_manager.c"
        "udp_server.c"
        "feature_extractor.c"
        "packet_capture.c"
        "packet_parser.c"
        "flow_table.c"
        "tcp_server.c"
    INCLUDE_DIRS "."
)
