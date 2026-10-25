#include "tcp_server.h"

#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lwip/sockets.h"

#include "feature_extractor.h"

#define TCP_SERVER_PORT          5006
#define RX_BUFFER_SIZE           128
#define KEEPALIVE_IDLE           5
#define KEEPALIVE_INTERVAL       5
#define KEEPALIVE_COUNT          3
#define FLOW_WINDOW_PACKET_LIMIT 5

static const char *TAG = "TCP_SERVER";

static void handle_client(int sock, const struct sockaddr_in *src)
{
    char rx[RX_BUFFER_SIZE];
    char src_ip[32];
    char dst_ip[32] = "0.0.0.0";

    inet_ntoa_r(src->sin_addr, src_ip, sizeof(src_ip));

    ESP_LOGI(TAG, "New client connected from %s:%d", src_ip, ntohs(src->sin_port)); // Logging when a client connects

    struct sockaddr_in local;
    socklen_t local_len = sizeof(local);
    if (getsockname(sock, (struct sockaddr *)&local, &local_len) == 0) {
        inet_ntoa_r(local.sin_addr, dst_ip, sizeof(dst_ip));
    }

    flow_features_t flow;
    feature_extractor_init(&flow, dst_ip, TCP_SERVER_PORT, "TCP");

    while (1) {
        int len = recv(sock, rx, sizeof(rx) - 1, 0);
        if (len < 0) {
            ESP_LOGE(TAG, "recv failed: errno %d", errno);
            break;
        }
        if (len == 0) {
            ESP_LOGW(TAG, "Connection closed by %s", src_ip);
            break;
        }
        rx[len] = '\0';

        feature_extractor_update(&flow, src_ip, ntohs(src->sin_port), len);
        feature_extractor_print(&flow, rx);

        if (flow.in_pkts >= FLOW_WINDOW_PACKET_LIMIT)
        {
            feature_extractor_print_ml_vector(&flow);
            feature_extractor_detect(&flow);
            feature_extractor_reset_window(&flow);
        }
    }
}

static void tcp_server_task(void *pvParameters)
{
    struct sockaddr_in dest = {0};
    dest.sin_family      = AF_INET;
    dest.sin_addr.s_addr = htonl(INADDR_ANY);
    dest.sin_port        = htons(TCP_SERVER_PORT);

    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(listen_sock, (struct sockaddr *)&dest, sizeof(dest)) != 0) {
        ESP_LOGE(TAG, "Bind failed: errno %d", errno);
        goto CLEAN_UP;
    }
    if (listen(listen_sock, 1) != 0) {
        ESP_LOGE(TAG, "Listen failed: errno %d", errno);
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "TCP server listening on port %d", TCP_SERVER_PORT);

    while (1) {
        struct sockaddr_in src;
        socklen_t src_len = sizeof(src);
        int sock = accept(listen_sock, (struct sockaddr *)&src, &src_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "accept failed: errno %d", errno);
            break;
        }

        int ka = 1, idle = KEEPALIVE_IDLE, intvl = KEEPALIVE_INTERVAL, cnt = KEEPALIVE_COUNT;
        setsockopt(sock, SOL_SOCKET,  SO_KEEPALIVE,  &ka,    sizeof(ka));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE,  &idle,  sizeof(idle));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT,   &cnt,   sizeof(cnt));

        handle_client(sock, &src);

        shutdown(sock, SHUT_RDWR);
        close(sock);
    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}

void tcp_server_start(void)
{
    ESP_LOGI(TAG, "Starting TCP server...");

    xTaskCreate(tcp_server_task, "tcp_server", 4096, NULL, 5, NULL);
}