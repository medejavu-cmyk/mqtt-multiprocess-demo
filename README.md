# MQTT 风格多进程通信实验

使用 C 语言与 Linux Socket 实现客户端和服务器通信，展示 `fork()` 多进程并发、匿名管道传递客户端序号，以及自定义 CONNECT/CONNACK 风格报文处理。

这是课程实验性质的协议模拟，不是完整 MQTT Broker；不能宣称兼容标准 MQTT 客户端、订阅发布或生产部署。

## 运行环境

Linux（或 Windows 下自行配置的 WSL），需要 GCC 和 Make。Windows PowerShell 不能直接编译这些 POSIX 接口。

在 Linux 项目目录运行：

```sh
make
./mqtt_server
```

另开终端进入同一目录：

```sh
./mqtt_client 127.0.0.1 1883 demo-client-01
./mqtt_client 127.0.0.1 1883 demo-client-02
```

第三个参数用于演示客户端标识，无需填写真实学号。服务端返回分配的客户端序号，服务端日志展示进程与管道处理过程。用 Ctrl+C 停止服务。

## 文件

- `mqtt_server.c`：accept、fork 与管道调度。
- `mqtt_client.c`：发送请求并显示响应。
- `mqtt_protocol.c/.h`：报文结构、序列化及读写辅助函数。
- `Makefile`：编译和清理目标。

## 状态与边界

由个人课程实验资料整理。公开候选仅含源码和说明，不含报告、真实学号或运行日志。本次整理将服务端监听范围改为本机回环地址；未加入认证、TLS、连接配额或超时控制。

尚未在 Linux 完成编译和运行验证。未授予明确开源许可证，待作者确认本项目代码来源及可授权范围后补充；公开展示不代表自动授予复制或再分发许可。
