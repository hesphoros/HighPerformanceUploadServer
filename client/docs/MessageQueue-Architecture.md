# 消息队列与连接监测架构

## 概述

本文档介绍 IPC 客户端的消息队列和连接监测机制，实现了**高性能无锁持久化队列**和**智能连接状态监测**。

## 核心组件

---

### 2. （连接监测器）

#### 设计目标
- ✅ **智能错误识别**：区分网络错误 vs 逻辑错误
- ✅ **状态机管理**：清晰的连接状态转换
- ✅ **自动触发重连**：网络错误自动触发重连
- ✅ **统计信息**：连续失败次数、最后活跃时间

#### 连接状态机

```
Disconnected ──connect()──> Connecting
      ↑                         │
      │                         ↓
      │                    Connected
      │                         │
      └───── Failed ←─── Reconnecting
```

#### 错误判断逻辑

**网络错误（需要重连）：**
- `connection_refused`（10061）
- `connection_reset`（10054）
- `connection_aborted`（10053）
- `network_unreachable`（10051）
- `host_unreachable`（10065）
- `eof`（连接关闭）
- `not_connected`（未连接）

**逻辑错误（不需要重连）：**
- 应用层协议错误
- 数据格式错误
- 其他非网络错误

#### 关键方法

```cpp
// 记录发送成功
connection_monitor_->record_send_success();

// 记录发送失败，返回是否需要重连
bool need_reconnect = connection_monitor_->record_send_failure(error_code);

// 设置回调
connection_monitor_->set_reconnect_callback([this]() {
    try_reconnect();
});
```

---

### 3. Lusp_AsioLoopbackIpcClient（集成）

#### 改进点

**1. 发送失败触发重连**
```cpp
void handle_send_result(const std::error_code& ec, size_t bytes_transferred) {
    if (!ec) {
        connection_monitor_->record_send_success();
        do_send_from_queue();  // 继续发送下一条
    } else {
        bool need_reconnect = connection_monitor_->record_send_failure(ec);
        if (!need_reconnect) {
            do_send_from_queue();  // 非网络错误，继续发送
        }
        // 网络错误会自动触发重连回调
    }
}
```

**2. 消息自动入队**
```cpp
void send(const std::string& message, uint32_t priority = 0) {
    std::vector<uint8_t> data(message.begin(), message.end());
    IpcMessage ipc_message(0, data, priority);
    
    if (!message_queue_->enqueue(std::move(ipc_message))) {
        // 队列满，记录错误
    }
    
    do_send_from_queue();  // 触发发送
}
```

**3. 连接成功后自动发送积压消息**
```cpp
void handle_connect_result(const std::error_code& ec, const endpoint& ep) {
    if (!ec) {
        connection_monitor_->set_state(ConnectionState::Connected);
        do_read();
        do_send_from_queue();  // 发送队列中的消息
    }
}
```

**4. 无锁发送控制**
```cpp
std::atomic<bool> is_sending_{false};

void do_send_from_queue() {
    bool expected = false;
    if (!is_sending_.compare_exchange_strong(expected, true)) {
        return;  // 已经在发送中
    }
    
    // 发送逻辑...
}
```

---

## 工作流程

### 消息发送流程

```
1. 用户调用 send(message)
   ↓
2. 消息入队（无锁）
   ↓
3. 触发 do_send_from_queue()
   ↓
4. 检查 is_sending_ 标志
   ↓
5. 从队列取出消息
   ↓
6. 执行 async_write
   ↓
7. 回调 handle_send_result()
   ├─ 成功 → record_send_success() → 发送下一条
   └─ 失败 → record_send_failure()
       ├─ 网络错误 → 触发重连回调
       └─ 逻辑错误 → 发送下一条
```

### 重连流程

```
1. 发送失败 → record_send_failure()
   ↓
2. is_connection_error() 判断错误类型
   ↓
3. 网络错误 → set_state(Reconnecting)
   ↓
4. 触发 reconnect_callback_()
   ↓
5. try_reconnect() 执行重连逻辑
   ↓
6. 连接成功 → do_send_from_queue() 发送积压消息
```

---

## 性能特点

### 1. 无锁设计优势
- **高并发**：生产者和消费者无竞争
- **低延迟**：无锁等待，CPU 缓存友好
- **可预测**：无死锁/优先级反转风险

### 2. 内存+磁盘策略
- **内存优先**：常见场景下全内存操作
- **自动溢出**：内存满后透明切换到磁盘
- **恢复保障**：程序崩溃后可恢复

### 3. 智能重连
- **精准判断**：只对网络错误重连
- **避免误判**：逻辑错误不触发重连
- **状态清晰**：状态机管理连接生命周期

---

## 监控与调试

### 队列统计信息

```cpp
auto stats = client->get_queue_statistics();

LOG_INFO("Queue Statistics:");
LOG_INFO("  Memory size: {}", stats.memory_size);
LOG_INFO("  Disk size: {}", stats.disk_size);
LOG_INFO("  Total enqueued: {}", stats.total_enqueued);
LOG_INFO("  Total dequeued: {}", stats.total_dequeued);
LOG_INFO("  Disk bytes: {}", stats.disk_bytes);
```

### 日志输出

- **MessageQueue.log**：队列操作日志
- **AsioLoopbackIpcClient.log**：IPC 客户端日志

### 关键日志

```log
[INFO] PersistentMessageQueue initialized, loaded 10 messages from disk
[INFO] 连接状态变化: Disconnected -> Connecting
[INFO] 连接状态变化: Connecting -> Connected
[WARN] Connection error detected: Connection refused (10061)
[INFO] 连接状态变化: Connected -> Reconnecting
[INFO] 消息 123 发送成功，长度: 1024
```

---

## 待实现功能

### 问题三：心跳机制（暂不实现）
- 应用层 PING-PONG
- TCP KeepAlive
- 连接活跃度检测

### 问题四：永久停止恢复（暂不实现）
- 定期自动恢复
- 手动触发恢复
- 智能三阶段恢复

---

## 配置示例

目前配置由代码硬编码，未来可添加到 `upload_client.toml`：

```toml
[MessageQueue]
memoryCapacity = 1024              # 内存队列容量
maxDiskSize = 104857600            # 磁盘最大 100MB
persistDir = "./queue"             # 持久化目录

[ConnectionMonitor]
enableAutoReconnect = true         # 启用自动重连
recordStatistics = true            # 记录统计信息
```

---

## 总结

本次实现完成了：

✅ **问题一（方案 1C）**：连接监测 + 队列
   - ConnectionMonitor 智能判断错误类型
   - 发送失败自动触发重连
   - 消息队列缓存待发送数据

✅ **问题二（方案 2C）**：高性能无锁持久化队列
   - SPSC 环形缓冲区（无锁）
   - 内存满后溢出到磁盘
   - 支持崩溃后恢复

未来扩展方向：
- 心跳机制（问题三）
- 永久停止恢复策略（问题四）
- 消息优先级调度
- 更多统计指标

---

**文档版本**: 1.0  
**最后更新**: 2025-10-02  
**作者**: GitHub Copilot
