/*
 * mqtt_protocol.c -- MQTT协议辅助函数实现
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "mqtt_protocol.h"

/* 循环读取 n 字节，处理信号中断和短读 */
int read_full(int fd, void *buf, size_t n)
{
    size_t  left = n;
    char   *ptr  = (char *)buf;
    while (left > 0) {
        ssize_t rc = read(fd, ptr, left);
        if (rc < 0) { perror("read"); return -1; }
        if (rc == 0) break;
        left -= rc;
        ptr  += rc;
    }
    return (n - left);
}

/* 循环写入 n 字节 */
int write_full(int fd, const void *buf, size_t n)
{
    size_t       left = n;
    const char  *ptr  = (const char *)buf;
    while (left > 0) {
        ssize_t rc = write(fd, ptr, left);
        if (rc < 0) { perror("write"); return -1; }
        left -= rc;
        ptr  += rc;
    }
    return n;
}

/* 构建CONNECT负载: client_id_length(2B) + client_id(学号) */
void build_connect_payload(const char *student_id,
                           char *payload, uint16_t *payload_len)
{
    uint16_t id_len = (uint16_t)strlen(student_id);
    payload[0] = (id_len >> 8) & 0xFF;
    payload[1] =  id_len       & 0xFF;
    memcpy(payload + 2, student_id, id_len);
    *payload_len = 2 + id_len;
}

/* 发送完整CONNECT报文: 固定报头 + 可变报头(10B) + 负载 */
int send_connect_packet(int sockfd, const char *student_id)
{
    mqtt_fixed_header_t  fh;
    mqtt_connect_var_t   vh;
    char                 payload[256];
    uint16_t             payload_len;
    uint8_t              remaining;
    char                 buf[512];
    size_t               offset = 0;

    build_connect_payload(student_id, payload, &payload_len);

    fh.packet_type      = MQTT_CONNECT;
    remaining            = sizeof(vh) + payload_len;
    fh.remaining_length  = remaining;

    memset(&vh, 0, sizeof(vh));
    vh.protocol_name_len_msb = 0x00;
    vh.protocol_name_len_lsb = 0x04;
    memcpy(vh.protocol_name, "MQTT", 4);
    vh.protocol_level  = 0x04;      /* MQTT 3.1.1 */
    vh.connect_flags   = 0x02;      /* Clean Session */
    vh.keep_alive_msb  = 0x00;
    vh.keep_alive_lsb  = 0x3C;      /* 60秒 */

    memcpy(buf + offset, &fh, sizeof(fh));        offset += sizeof(fh);
    memcpy(buf + offset, &vh, sizeof(vh));        offset += sizeof(vh);
    memcpy(buf + offset, payload, payload_len);   offset += payload_len;

    return write_full(sockfd, buf, offset);
}

/* 发送完整CONNACK报文: 固定报头(2B) + 可变报头(2B) + 负载(client_seq 4B) */
int send_connack_packet(int sockfd, uint32_t client_seq)
{
    mqtt_connack_packet_t  pkt;
    connack_payload_t      pl;
    char buf[64];
    int  offset = 0;
    uint8_t remaining;

    memset(&pkt, 0, sizeof(pkt));
    pkt.fixed_header.packet_type     = MQTT_CONNACK;
    remaining                         = sizeof(pkt.var_header) + sizeof(pl);
    pkt.fixed_header.remaining_length = remaining;
    pkt.var_header.session_present    = 0x00;
    pkt.var_header.return_code        = CONNACK_ACCEPTED;

    pl.client_sequence = htonl(client_seq);

    memcpy(buf + offset, &pkt, sizeof(pkt));   offset += sizeof(pkt);
    memcpy(buf + offset, &pl,  sizeof(pl));    offset += sizeof(pl);

    return write_full(sockfd, buf, offset);
}

/* 接收并解析CONNECT报文，提取学号 */
int recv_connect_packet(int sockfd, char *student_id_buf, size_t buf_size)
{
    mqtt_fixed_header_t  fh;
    mqtt_connect_var_t   vh;
    uint16_t             id_len;

    if (read_full(sockfd, &fh, sizeof(fh)) != (int)sizeof(fh))
        return -1;
    if (fh.packet_type != MQTT_CONNECT) {
        fprintf(stderr, "Expected CONNECT (0x10), got 0x%02X\n", fh.packet_type);
        return -1;
    }
    if (read_full(sockfd, &vh, sizeof(vh)) != (int)sizeof(vh))
        return -1;

    uint8_t len_buf[2];
    if (read_full(sockfd, len_buf, 2) != 2)
        return -1;
    id_len = ((uint16_t)len_buf[0] << 8) | len_buf[1];

    if (id_len >= buf_size) {
        fprintf(stderr, "Student ID too long: %u\n", id_len);
        return -1;
    }
    if (read_full(sockfd, student_id_buf, id_len) != (int)id_len)
        return -1;
    student_id_buf[id_len] = '\0';
    return 0;
}

/* 接收并解析CONNACK，提取客户端序号 */
int recv_connack_packet(int sockfd, uint32_t *client_seq)
{
    mqtt_fixed_header_t  fh;
    mqtt_connack_var_t   vh;
    connack_payload_t    pl;

    if (read_full(sockfd, &fh, sizeof(fh)) != (int)sizeof(fh))
        return -1;
    if (fh.packet_type != MQTT_CONNACK) {
        fprintf(stderr, "Expected CONNACK (0x20), got 0x%02X\n", fh.packet_type);
        return -1;
    }
    if (read_full(sockfd, &vh, sizeof(vh)) != (int)sizeof(vh))
        return -1;
    if (read_full(sockfd, &pl, sizeof(pl)) != (int)sizeof(pl))
        return -1;

    *client_seq = ntohl(pl.client_sequence);
    return 0;
}