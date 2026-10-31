/*
 * mqtt_server.c -- 多进程 MQTT 并行服务器
 *
 * 功能:
 *   1. 主进程监听TCP端口, 每收到一个客户端连接就fork子进程
 *   2. 通过无名管道将当前客户端序号传递给子进程
 *   3. 子进程接收客户端CONNECT报文, 提取学号并打印
 *   4. 子进程将客户端序号作为CONNACK负载发回客户端
 *
 * 编译: gcc -o mqtt_server mqtt_server.c mqtt_protocol.c -Wall
 * 使用: ./mqtt_server [port]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include "mqtt_protocol.h"

#define DEFAULT_PORT      1883
#define BACKLOG           10

static volatile sig_atomic_t running = 1;

/* SIGINT/SIGTERM 处理: 优雅退出 */
static void handle_signal(int sig)
{
    (void)sig;
    running = 0;
}

/* SIGCHLD 处理: 回收僵尸进程 */
static void handle_sigchld(int sig)
{
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

int main(int argc, char *argv[])
{
    int                 listen_fd, client_fd;
    struct sockaddr_in  serv_addr, cli_addr;
    socklen_t           cli_len = sizeof(cli_addr);
    uint16_t            port    = DEFAULT_PORT;
    uint32_t            client_count = 0;

    if (argc >= 2) port = (uint16_t)atoi(argv[1]);

    /* ---------- 1. 注册信号处理 ---------- */
    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGCHLD, handle_sigchld);

    /* ---------- 2. 创建监听套接字 ---------- */
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); exit(EXIT_FAILURE); }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    serv_addr.sin_port        = htons(port);

    if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind"); exit(EXIT_FAILURE);
    }

    if (listen(listen_fd, BACKLOG) < 0) {
        perror("listen"); exit(EXIT_FAILURE);
    }

    printf("[MQTT Server] Listening on port %u ...\n", port);

    /* ---------- 3. 主循环: accept + fork ---------- */
    while (running) {
        client_fd = accept(listen_fd, (struct sockaddr *)&cli_addr, &cli_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        client_count++;

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli_addr.sin_addr, client_ip, sizeof(client_ip));
        printf("[MQTT Server] Client #%u from %s:%d\n",
               client_count, client_ip, ntohs(cli_addr.sin_port));

        /* ---------- 4. 创建无名管道 ---------- */
        int pipefd[2];
        if (pipe(pipefd) < 0) {
            perror("pipe");
            close(client_fd);
            continue;
        }

        /* ---------- 5. fork 子进程 ---------- */
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(pipefd[0]); close(pipefd[1]);
            close(client_fd);
            continue;
        }

        if (pid == 0) {
            /* ===== 子进程 ===== */
            close(listen_fd);
            close(pipefd[1]);   /* 关闭写端 */

            /* 从管道读取父进程传来的客户端序号 */
            uint32_t my_seq;
            if (read_full(pipefd[0], &my_seq, sizeof(my_seq))
                != (int)sizeof(my_seq)) {
                fprintf(stderr, "[Child] Failed to read pipe\n");
                close(pipefd[0]); close(client_fd);
                exit(EXIT_FAILURE);
            }
            close(pipefd[0]);

            printf("[Child #%u] Ready, waiting for CONNECT...\n", my_seq);

            /* 接收客户端发来的CONNECT报文(含学号) */
            char student_id[64];
            if (recv_connect_packet(client_fd, student_id,
                                    sizeof(student_id)) < 0) {
                fprintf(stderr, "[Child #%u] Failed to recv CONNECT\n", my_seq);
                close(client_fd);
                exit(EXIT_FAILURE);
            }

            printf("[Child #%u] Received Student ID: %s\n", my_seq, student_id);

            /* 向客户端发送CONNACK, 负载为该客户端序号 */
            if (send_connack_packet(client_fd, my_seq) < 0) {
                fprintf(stderr, "[Child #%u] Failed to send CONNACK\n", my_seq);
                close(client_fd);
                exit(EXIT_FAILURE);
            }

            printf("[Child #%u] Sent sequence=%u to client, exiting\n",
                   my_seq, my_seq);

            close(client_fd);
            exit(EXIT_SUCCESS);
        }
        else {
            /* ===== 父进程 ===== */
            close(client_fd);       /* 父进程关闭客户套接字 */
            close(pipefd[0]);       /* 关闭读端 */

            /* 将当前客户端计数写入管道 */
            if (write_full(pipefd[1], &client_count, sizeof(client_count))
                != (int)sizeof(client_count)) {
                fprintf(stderr, "[Parent] Failed to write pipe\n");
            }
            close(pipefd[1]);

            printf("[Parent] Sent count=%u to child via pipe\n", client_count);
        }
    }

    /* ---------- 6. 清理 ---------- */
    printf("\n[MQTT Server] Shutting down...\n");
    close(listen_fd);

    while (waitpid(-1, NULL, 0) > 0)
        ;

    printf("[MQTT Server] Closed. Served %u clients total.\n", client_count);
    return 0;
}