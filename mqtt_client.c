/*
 * mqtt_client.c -- MQTT 模拟客户端
 *
 * 功能:
 *   1. 连接MQTT服务器
 *   2. 发送CONNECT报文, 负载为学号
 *   3. 接收CONNACK, 打印服务器分配的客户端序号
 *
 * 编译: gcc -o mqtt_client mqtt_client.c mqtt_protocol.c -Wall
 * 使用: ./mqtt_client <server_ip> [port] [student_id]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "mqtt_protocol.h"

#define DEFAULT_PORT      1883
#define DEFAULT_STUDENT_ID "20240001"

int main(int argc, char *argv[])
{
    const char *server_ip;
    uint16_t    port;
    const char *student_id;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <server_ip> [port] [student_id]\n", argv[0]);
        fprintf(stderr, "Example: %s 127.0.0.1 1883 20240001\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    server_ip  = argv[1];
    port       = (argc >= 3) ? (uint16_t)atoi(argv[2]) : DEFAULT_PORT;
    student_id = (argc >= 4) ? argv[3] : DEFAULT_STUDENT_ID;

    /* 创建套接字并连接 */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); exit(EXIT_FAILURE); }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(port);

    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid IP: %s\n", server_ip);
        exit(EXIT_FAILURE);
    }

    printf("[MQTT Client] Connecting to %s:%u ...\n", server_ip, port);
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect"); exit(EXIT_FAILURE);
    }
    printf("[MQTT Client] Connected to server\n");

    /* 发送CONNECT报文(含学号) */
    printf("[MQTT Client] Sending CONNECT with Student ID: %s\n", student_id);
    if (send_connect_packet(sockfd, student_id) < 0) {
        fprintf(stderr, "Failed to send CONNECT\n");
        close(sockfd); exit(EXIT_FAILURE);
    }

    /* 接收CONNACK(含客户端序号) */
    uint32_t client_seq = 0;
    if (recv_connack_packet(sockfd, &client_seq) < 0) {
        fprintf(stderr, "Failed to receive CONNACK\n");
        close(sockfd); exit(EXIT_FAILURE);
    }

    printf("========================================\n");
    printf("  Student ID: %s\n", student_id);
    printf("  You are client #%u\n", client_seq);
    printf("========================================\n");

    close(sockfd);
    return 0;
}