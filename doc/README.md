# 高性能文件上传服务器架构文档

## 概述

基于原始需求描述的高性能文件上传客户端架构设计，支持最大50个并发上传，引用计数管理，Web隔离调用等特性。

## 原始需求核心要点

1. **客户端职责明确**：只启动通知线程，主线程将数据丢进队列
2. **行级锁队列**：只锁出队和入队操作，使用条件变量唤醒线程
3. **本地服务独立**：接收通知，管理上传队列，启动后台服务
4. **阻塞上传模式**：一个线程处理一个文件，阻塞等待完成
5. **引用计数控制**：最大50个并发，满了则排队等待
6. **协议自适应**：局域网使用ZeroMQ，广域网使用ASIO
7. **Web隔离支持**：Web调用需要绕弯，通过桥接层转发
8. **定时进度回调**：每秒返回进度，通过回调显示在UI

## 架构分层

```
┌─────────────────────────────────────────────────────────────┐
│                    调用层 (Application Layer)               │
│  - 本地应用 (Qt UI、控制台、C++程序)                        │
│  - Web应用 (HTTP API、浏览器、远程调用)                     │
├─────────────────────────────────────────────────────────────┤
│                  SDK层 (SDK Layer)                         │
│  - FileUploadSDK (统一接口)                                │
│  - 引用计数管理器 (最大50并发)                              │
│  - Web桥接服务器 (Web隔离支持)                             │
├─────────────────────────────────────────────────────────────┤
│                通知层 (Notification Layer)                  │
│  - 通知线程 (行级锁队列)                                    │
│  - 等待队列管理器                                           │
│  - 进程间通信 (Named Pipe)                                 │
├─────────────────────────────────────────────────────────────┤
│               本地服务层 (Local Service Layer)              │
│  - 独立服务进程                                             │
│  - 上传队列管理                                             │
│  - 线程池管理器                                             │
├─────────────────────────────────────────────────────────────┤
│              上传执行层 (Upload Execution Layer)            │
│  - 阻塞上传线程池 (最大50个)                               │
│  - 网络协议适配器 (ZeroMQ/ASIO)                           │
│  - 进度报告器 (1秒定时器)                                  │
└─────────────────────────────────────────────────────────────┘
```

## 核心特性

- ✅ **高并发**：支持最大50个文件同时上传
- ✅ **引用计数**：精确控制并发数量，超出限制自动排队
- ✅ **Web隔离**：支持Web应用通过HTTP/WebSocket调用
- ✅ **协议自适应**：自动选择最优网络协议
- ✅ **实时进度**：每秒回调上传进度
- ✅ **高可靠性**：独立服务进程，故障隔离
- ✅ **低延迟**：异步非阻塞设计，UI响应迅速

## 模块文档

| 模块 | 文档路径 | 描述 |
|------|----------|------|
| SDK层 | [modules/01-sdk-layer.md](modules/01-sdk-layer.md) | 统一SDK接口，引用计数管理 |
| 通知层 | [modules/02-notification-layer.md](modules/02-notification-layer.md) | 通知线程，行级锁队列 |
| 本地服务层 | [modules/03-local-service-layer.md](modules/03-local-service-layer.md) | 独立服务进程，上传管理 |
| 上传执行层 | [modules/04-upload-execution-layer.md](modules/04-upload-execution-layer.md) | 阻塞上传线程，网络协议 |
| Web桥接层 | [modules/05-web-bridge-layer.md](modules/05-web-bridge-layer.md) | Web隔离支持，HTTP/WebSocket |
| 网络协议层 | [modules/06-network-protocol-layer.md](modules/06-network-protocol-layer.md) | ZeroMQ/ASIO适配器 |

## API文档

| API | 文档路径 | 描述 |
|-----|----------|------|
| SDK API | [api/sdk-api.md](api/sdk-api.md) | SDK接口规范 |
| Web API | [api/web-api.md](api/web-api.md) | HTTP/WebSocket API |
| 回调接口 | [api/callback-api.md](api/callback-api.md) | 进度回调接口 |

## 架构图

| 图表 | 文档路径 | 描述 |
|------|----------|------|
| 整体架构图 | [diagrams/overall-architecture.md](diagrams/overall-architecture.md) | 系统整体架构 |
| 引用计数流程图 | [diagrams/reference-count-flow.md](diagrams/reference-count-flow.md) | 引用计数管理流程 |
| Web隔离调用图 | [diagrams/web-isolation-flow.md](diagrams/web-isolation-flow.md) | Web隔离调用流程 |
| 网络协议选择图 | [diagrams/network-protocol-selection.md](diagrams/network-protocol-selection.md) | 协议自适应逻辑 |

## 快速开始

1. 查看 [整体架构图](diagrams/overall-architecture.md) 了解系统架构
2. 阅读 [SDK层文档](modules/01-sdk-layer.md) 了解接口使用
3. 参考 [API文档](api/sdk-api.md) 进行开发集成

## 版本历史

- v1.0 - 初始版本，基于原始需求描述设计
- v1.1 - 添加Web隔离支持
- v1.2 - 完善引用计数机制
- v1.3 - 优化网络协议选择

---

*文档生成时间：2025年6月21日*
