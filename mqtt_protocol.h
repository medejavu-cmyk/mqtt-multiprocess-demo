/*
 * mqtt_protocol.h -- MQTT协议结构体定义
 * 模拟MQTT 3.1.1协议的CONNECT/CONNACK报文结构
 */

#ifndef MQTT_PROTOCOL_H
#define MQTT_PROTOCOL_H

#include <stdint.h>

/* ========== MQTT 控制报文类型 ========== */
#define MQTT_CONNECT     0x10   /* 连接请求 */
#define MQTT_CONNACK     0x20   /* 连接确认 */
#define MQTT_PUBLISH     0x30   /* 发布消息 */
#define MQTT_DISCONNECT  0xE0   /* 断开连接 */

/* ========== CONNACK 返回码 ========== */
#define CONNACK_ACCEPTED         0x00
#define CONNACK_REFUSED_PROTO    0x01
#define CONNACK_REFUSED_ID       0x02
#define CONNACK_REFUSED_SERVER   0x03

/* ========== 固定报头结构 ========== */
typedef struct {
    uint8_t  packet_type;
    uint8_t  remaining_length;
} __attribute__((packed)) mqtt_fixed_header_t;

/* ========== CONNECT 可变报头 ========== */
typedef struct {
    uint8_t  protocol_name_len_msb;
    uint8_t  protocol_name_len_lsb;
    char     protocol_name[4];       /* "MQTT" */
    uint8_t  protocol_level;         /* 协议级别 4=MQTT 3.1.1 */
    uint8_t  connect_flags;
    uint8_t  keep_alive_msb;
    uint8_t  keep_alive_lsb;
} __attribute__((packed)) mqtt_connect_var_t;

/* ========== CONNECT 报文完整结构 ========== */
typedef struct {
    mqtt_fixed_header_t  fixed_header;
    mqtt_connect_var_t   var_header;
    /* Payload紧随其后: uint16_t client_id_len + char client_id[] */
} __attribute__((packed)) mqtt_connect_packet_t;

/* ========== CONNACK 可变报头 ========== */
typedef struct {
    uint8_t  session_present;
    uint8_t  return_code;
} __attribute__((packed)) mqtt_connack_var_t;

/* ========== CONNACK 报文完整结构 ========== */
typedef struct {
    mqtt_fixed_header_t  fixed_header;
    mqtt_connack_var_t   var_header;
} __attribute__((packed)) mqtt_connack_packet_t;

/* ========== 自定义响应负载: 客户端序号 ========== */
typedef struct {
    uint32_t client_sequence;
} __attribute__((packed)) connack_payload_t;

/* ========== 辅助函数声明 ========== */
void build_connect_payload(const char *student_id,
                           char *payload, uint16_t *payload_len);
void build_connack(uint32_t client_seq,
                   mqtt_connack_packet_t *connack,
                   connack_payload_t *payload);
int send_connect_packet(int sockfd, const char *student_id);
int send_connack_packet(int sockfd, uint32_t client_seq);
int recv_connect_packet(int sockfd, char *student_id_buf, size_t buf_size);
int recv_connack_packet(int sockfd, uint32_t *client_seq);
int read_full(int fd, void *buf, size_t n);
int write_full(int fd, const void *buf, size_t n);

#endif /* MQTT_PROTOCOL_H */