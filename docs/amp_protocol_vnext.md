# AMP vNext（v4）协议

## 1. 文档状态

- 状态：v4 Raw HID 首版已实现；其他 Transport Adapter 保留接口
- 适用项目：libamp、Oholeo Keyboard V2 Firmware、EMI Keyboard Configurator
- 兼容策略：不兼容现有 AMP 帧格式，不提供旧协议兼容层
- 固定协议标识：`AMP_FRAME_PROTO = 0x41`
- 当前线协议版本：`AMP_WIRE_VERSION = 4`

本文档定义从零推导后的 AMP vNext。设计不受当前 Raw HID 帧格式、现有 Packet ABI 或迁移成本约束。

## 2. 设计目标

AMP vNext 应满足以下目标：

1. 协议核心不依赖 Raw HID，可运行于 Raw HID、USB Bulk、UART、BLE 或 TCP。
2. 明确区分线协议版本、固件版本和配置文档版本。
3. 使用显式会话替代 `GET Version` 会话起始启发式。
4. 支持可靠的请求/响应关联、超时重试和重复请求去重。
5. 配置读取获得稳定快照；支持原子提交的平台可以为配置保存提供原子提交语义。
6. 页面关闭、传输中断或重新连接后不会遗留半完成事务。
7. Debug、Console 等流式数据不能阻塞控制和配置通信。
8. 配置结构能够增加新 Section、新记录和新 DynamicKey 类型。
9. 固件内存使用仍然可预测，不要求动态分配大块内存。

以下内容不属于核心目标：

- 不提供身份认证、权限控制和数据加密，上位机仍被视为可信端。
- 不在 AMP 核心中重复实现所有传输已有的可靠性机制。
- 不使用 JSON、CBOR 等通用自描述格式替代紧凑的二进制配置格式。

## 3. 分层模型

协议划分为四层：

```text
配置器 / 键盘业务逻辑
          │
          ▼
服务层：Control / Config / Object / Debug / Console
          │
          ▼
AMP 消息层：Header、请求响应、会话、状态码
          │
          ▼
Transport Adapter：Raw HID / USB Bulk / UART / BLE / TCP
```

AMP 消息层不调用特定 USB API。Transport Adapter 负责把完整 AMP 消息交给消息层，并负责其传输所需的分片、重组、转义或校验。

## 4. AMP 消息头

### 4.1 结构定义

```c
typedef struct
{
    uint8_t  proto;
    uint8_t  version;
    uint8_t  channel_flags;
    uint8_t  status;
    uint16_t session_id;
    uint16_t request_id;
    uint16_t opcode;
    uint16_t payload_len;
} __PACKED AmpHeader;
```

所有多字节整数使用小端序。Header 固定为 12 字节。

| 偏移 | 大小 | 字段 | 语义 |
| ---: | ---: | --- | --- |
| 0 | 1 | `proto` | 固定为 `0x41` |
| 1 | 1 | `version` | AMP 线协议版本 |
| 2 | 1 | `channel_flags` | 高 4 位 Channel，低 4 位 Flags |
| 3 | 1 | `status` | 响应状态；请求和事件必须为 0 |
| 4 | 2 | `session_id` | 当前通信会话，0 为无会话 |
| 6 | 2 | `request_id` | 请求和响应关联；0 保留给异步事件 |
| 8 | 2 | `opcode` | Channel 内的操作码 |
| 10 | 2 | `payload_len` | Header 后有效 Payload 的字节数 |

逻辑消息长度为：

```text
sizeof(AmpHeader) + payload_len
```

`payload_len` 是有效业务数据长度，不包含 Raw HID 补零、UART CRC 或其他 Transport Adapter 数据。

### 4.2 Flags

```c
enum
{
    AMP_FLAG_RESPONSE = 0x01,
    AMP_FLAG_EVENT    = 0x02,
};
```

- 普通请求：Flags 为 0，`request_id != 0`。
- 普通响应：设置 `RESPONSE`，回显请求的 Session、Request ID、Channel 和 Opcode。
- 异步事件：设置 `EVENT`，`request_id == 0`。
- `RESPONSE` 与 `EVENT` 不得同时设置。
- 其余两位保留，当前版本必须发送为 0。

所有普通请求都必须产生响应，因此不再保留 `REQ_ACK`。

### 4.3 Channel

Channel 表示服务和队列调度等级，不再与 `code/type` 共同描述 Packet 类型。

```c
typedef enum
{
    AMP_CHANNEL_CONTROL   = 0,
    AMP_CHANNEL_CONFIG    = 1,
    AMP_CHANNEL_OBJECT    = 2,
    AMP_CHANNEL_DEBUG     = 3,
    AMP_CHANNEL_CONSOLE   = 4,
    AMP_CHANNEL_TELEMETRY = 5,
    AMP_CHANNEL_USER      = 15,
} AmpChannel;
```

### 4.4 Status

```c
typedef enum
{
    AMP_STATUS_OK               = 0,
    AMP_STATUS_UNSUPPORTED      = 1,
    AMP_STATUS_INVALID_ARGUMENT = 2,
    AMP_STATUS_BUSY             = 3,
    AMP_STATUS_IO_ERROR         = 4,
    AMP_STATUS_INVALID_STATE    = 5,
    AMP_STATUS_CONFLICT         = 6,
    AMP_STATUS_STALE_REVISION   = 7,
    AMP_STATUS_NOT_FOUND        = 8,
    AMP_STATUS_INTEGRITY_ERROR  = 9,
    AMP_STATUS_ABORTED          = 10,
    AMP_STATUS_NO_SPACE         = 11,
} AmpStatus;
```

错误响应可以携带结构化错误 Payload，但不能依赖文本才能判断错误类型。

### 4.5 Opcode 表

Opcode 只在所属 Channel 内解释。v4 已固化以下取值：

| Channel | Opcode | 值 |
| --- | --- | ---: |
| Control | `CONTROL_HELLO` | `0x0001` |
| Control | `CONTROL_KEY_EVENT` | `0x0002` |
| Config | `CONFIG_ACTIVATE_PROFILE` | `0x0001` |
| Config event | `CONFIG_ACTIVE_PROFILE_CHANGED` | `0x8001` |
| Config event | `CONFIG_PROFILE_CHANGED` | `0x8002` |
| Object | `OBJECT_OPEN_READ` | `0x0001` |
| Object | `OBJECT_READ` | `0x0002` |
| Object | `OBJECT_CLOSE_READ` | `0x0003` |
| Object | `OBJECT_OPEN_WRITE` | `0x0004` |
| Object | `OBJECT_WRITE` | `0x0005` |
| Object | `OBJECT_COMMIT` | `0x0006` |
| Object | `OBJECT_ABORT` | `0x0007` |
| Debug | `DEBUG_SUBSCRIBE` | `0x0001` |
| Debug | `DEBUG_UNSUBSCRIBE` | `0x0002` |
| Debug | `DEBUG_SAMPLE` | `0x0003` |
| Debug event | `DEBUG_DATA` | `0x8001` |
| Console | `CONSOLE_SUBSCRIBE` | `0x0001` |
| Console | `CONSOLE_UNSUBSCRIBE` | `0x0002` |
| Console event | `CONSOLE_DATA` | `0x8001` |

`0x8000` 至 `0xffff` 保留给异步事件；普通请求 Opcode 使用
`0x0001` 至 `0x7fff`。

## 5. Transport Adapter

### 5.1 统一接口

Transport Adapter 至少提供以下抽象能力：

```c
int amp_transport_send(const uint8_t *message, uint16_t len);
void amp_transport_receive(const uint8_t *message, uint16_t len);
void amp_transport_reset_session(void);
void amp_transport_prepare_session(void);
uint16_t amp_transport_max_payload(void);
```

消息层接收的必须是一个完整 AMP 消息，不能接收任意半包。

### 5.2 Raw HID

Raw HID 每个物理 Report 固定为 64 字节：

```text
12 字节 AmpHeader + 最多 52 字节 Payload + 零填充
```

规则：

- `payload_len <= 52`。
- 发送始终为 64 字节。
- 接收时忽略 `12 + payload_len` 之后的填充数据。
- 一个 HID Report 对应一个 AMP 消息，不在 HID Adapter 中实现跨 Report 分片。
- 超过单帧 Payload 的对象通过 Object Service 分块传输。

### 5.3 USB Bulk 和 TCP

USB Bulk 与 TCP 只传输实际逻辑消息：

```text
12 + payload_len
```

接收端先读取固定 Header，再根据 `payload_len` 读取 Payload。TCP Adapter 必须处理一次读取包含多个消息或半个消息的情况。

### 5.4 UART

UART 使用外部 framing，不把 CRC 放入 AMP Header。推荐：

```text
COBS(AMP 消息 + CRC16) + 0x00
```

UART Adapter 负责：

- 消息定界；
- CRC16 校验；
- 丢弃损坏帧；
- 将验证后的完整 AMP 消息交给消息层。

### 5.5 BLE

BLE Adapter 根据 ATT MTU 对 AMP 消息分片和重组。分片序号、总长度等属于 BLE Adapter，不进入 AMP Header。

### 5.6 Payload 协商

双方在 HELLO 中公布接收能力。每个方向的最大 Payload 为：

```text
min(发送端最大值, 接收端最大值, 当前 Transport 最大值)
```

Object Service 的 Chunk 大小根据协商结果动态计算。

## 6. 会话模型

### 6.1 HELLO

连接建立后的第一条有效消息必须是：

```text
Channel: CONTROL
Opcode:  CONTROL_HELLO
Flags:   Request
Session: 上位机生成的非零随机 uint16_t
```

v4 首版的 HELLO Payload：

```c
typedef struct
{
    uint16_t max_rx_payload;
    uint16_t max_tx_payload;
    uint32_t capabilities;
} AmpHelloRequest;

typedef struct
{
    uint16_t max_rx_payload;
    uint16_t max_tx_payload;
    uint32_t capabilities;
    uint32_t device_state_revision;
    uint16_t active_profile;
    uint16_t profile_count;
    uint16_t firmware_major;
    uint16_t firmware_minor;
    uint16_t firmware_patch;
    uint16_t max_inflight_requests;
    uint16_t advanced_key_count;
    uint16_t total_key_count;
    uint16_t layer_count;
    uint16_t dynamic_key_count;
    uint16_t macro_count;
    uint16_t macro_action_count;
    uint16_t rgb_count;
    uint8_t  firmware_info_length;
    uint8_t  firmware_info[13];
} AmpHelloResponse;
```

Object Service 相关 capability：

```c
AMP_CAP_OBJECT_CRC32         = 1U << 5;
AMP_CAP_OBJECT_ATOMIC_COMMIT = 1U << 6;
```

- `OBJECT_CRC32` 表示设备生成并验证 Object 的 CRC32；未设置时 CRC 字段必须发送为 0，接收方不得校验。
- `OBJECT_ATOMIC_COMMIT` 表示写入暂存对象并在 Commit 时原子替换最终对象；未设置时写入可以直接修改最终对象，Abort 不能恢复旧内容。

当前 Response 为 52 字节，恰好占满 64 字节 Raw HID Report 的 Payload。若以后需要
支持单帧 Payload 小于 52 字节的 Transport，应给 HELLO 定义独立的最小响应或在
Transport 建链阶段先协商 MTU；这不改变 AMP Header。

方向以消息发送方为基准：设备公布的 `max_rx_payload` 是上位机的发送上限，设备公布
的 `max_tx_payload` 是上位机的接收上限。双方都必须再与本地 Transport 上限取最小值。

Header 的 `version` 表示请求使用的 AMP 线协议版本。设备不支持该版本时返回 Unsupported 或直接拒绝建立会话。

### 6.2 新会话处理

收到合法 HELLO 后，固件必须在处理该 HELLO 前：

1. 终止旧会话；
2. 清空旧 RX/TX 队列；
3. 取消旧的 Transport IN 传输；
4. Abort 所有未 Commit 的写事务；未声明原子提交能力的设备只保证关闭事务，不能保证恢复旧对象；
5. 关闭所有旧读快照；
6. 清理旧 Debug、Console 订阅；
7. 将当前 Session 设置为 HELLO 中的 `session_id`；
8. 再将 HELLO 交给消息层并返回响应。

AMP Session 层校验 HELLO 后调用 `amp_transport_prepare_session()`。Raw HID
Adapter 在该回调中取消遗留的 IN 传输，但不在 USB OUT 回调中解析 AMP Header。

即使 HELLO 使用与当前相同的 Session ID，也必须重新初始化会话，使重复 HELLO 可以作为显式恢复边界。

### 6.3 Session 过滤

会话建立后：

- 除 HELLO 外，只接受与当前 `session_id` 相同的消息。
- 所有响应和事件携带当前 `session_id`。
- 上位机忽略其他 Session 的响应和事件。
- USB Reset、物理断开或 Transport Reset 清除当前 Session。

## 7. 请求、响应与重试

### 7.1 Request ID

- `request_id == 0` 仅用于事件。
- 上位机为每个请求分配非零 uint16_t ID。
- 响应必须回显相同 ID。
- ID 回绕时跳过 0。
- 同一个 Session 内，未完成请求不得重用 ID。

### 7.2 并发

协议允许多个请求在途，但固件可以在 HELLO capability 中声明最大并发数。初始实现可以声明为 1，继续采用 stop-and-wait，不影响以后扩展。

### 7.3 重复请求

上位机超时重试必须重用原 `request_id`。固件对同一 Session 和 Request ID 的重复请求必须满足以下之一：

1. 返回已缓存的相同响应；或
2. 操作本身具有幂等语义，不重复产生副作用。

写 Chunk 使用 `transaction_id + offset` 定位，重复写入相同内容必须成功。Commit 的重复请求必须返回第一次 Commit 的最终结果。

## 8. 队列与 QoS

建议固件使用三个独立队列：

1. Response Queue：所有请求响应，最高优先级，可靠 FIFO。
2. Control Queue：配置变化等可靠事件，中优先级。
3. Stream Queue：Debug、Console、Telemetry，最低优先级，可丢弃旧数据。

调度顺序：

```text
Response > Control Event > Stream Event
```

Stream Queue 满时可以丢弃最旧帧，但不能占满 Response Queue，也不能阻止 HELLO、Commit 或 Abort。

新会话建立时三个队列全部清空。

## 9. Object Service

Object Service 统一替代现有 LargeTransfer，并作为配置、脚本和其他大对象的传输基础。

### 9.1 对象类型

```c
typedef enum
{
    AMP_OBJECT_CONFIG_PROFILE  = 1,
    AMP_OBJECT_SCRIPT_SOURCE   = 2,
    AMP_OBJECT_SCRIPT_BYTECODE = 3,
    AMP_OBJECT_USER_BASE       = 0x8000,
} AmpObjectType;
```

一个对象由 `object_type + object_id` 标识。配置对象的 `object_id` 为 Profile ID。

### 9.2 读取事务

```text
OBJECT_OPEN_READ(object_type, object_id)
    → transaction_id
    → revision
    → total_size
    → crc32

OBJECT_READ(transaction_id, offset, requested_length)
    → offset
    → data

OBJECT_CLOSE_READ(transaction_id)
```

`crc32` 字段始终存在。设备声明 `AMP_CAP_OBJECT_CRC32` 时填写完整对象的
IEEE CRC32；未声明时填写 0，上位机跳过 CRC 校验。

`OPEN_READ` 创建不可变快照。之后对象发生变化也不能改变当前事务返回的数据。

如果平台无法提供快照，任何变化都必须使后续读取返回 `STALE_REVISION`，不能混合两个版本的数据。

### 9.3 写入事务

```text
OBJECT_OPEN_WRITE(
    object_type,
    object_id,
    expected_revision,
    total_size,
    crc32)
    → transaction_id

OBJECT_WRITE(transaction_id, offset, data)

OBJECT_COMMIT(transaction_id)
    → new_revision

OBJECT_ABORT(transaction_id)
```

写入规则：

- `expected_revision` 不匹配时返回 `STALE_REVISION`。
- Chunk 必须从 offset 0 开始顺序写入；每个 Chunk 的 `offset` 必须等于事务的
  `next_offset`，否则返回 `INVALID_ARGUMENT`。
- 请求超时重试必须复用相同的 `request_id`，由请求响应缓存重发结果，不在 Object
  Service 中重复写入同一范围。
- Commit 前检查 `next_offset == total_size`；声明 `AMP_CAP_OBJECT_CRC32` 时还必须检查 CRC32。
- 声明 `AMP_CAP_OBJECT_ATOMIC_COMMIT` 时，数据写入临时对象，并在 Commit 时使用文件 rename、双槽或其他平台原子机制替换旧对象。
- 未声明 `AMP_CAP_OBJECT_ATOMIC_COMMIT` 时可以直接写最终对象；传输中断、Abort、校验失败或掉电都不能恢复旧内容。
- Commit 成功后增加对象 Revision。
- Session 结束时自动 Abort 所有未 Commit 写事务。
- Abort 始终关闭句柄；原子提交模式还会删除临时对象。

对象 CRC32 用于确认完整对象和存储内容，不替代 Transport CRC。固件通过
`AMP_OBJECT_CRC32_ENABLE` 控制该能力，通过 `AMP_OBJECT_TEMP_FILE_ENABLE`
控制临时文件原子提交；两个选项默认均为 1。

### 9.4 资源限制

固件通过 HELLO capability 公布：

- 最大同时读取事务数；
- 最大同时写入事务数；
- 最大对象大小；
- 支持的对象类型。

初始实现可以只允许一个读事务和一个写事务。

## 10. Config Service

### 10.1 Profile 状态

配置内容和当前激活 Profile 是两类不同状态：

- 每个 Profile 有独立 `profile_revision`，仅在配置内容 Commit 后增加。
- 设备有独立 `device_state_revision`，在激活 Profile 变化等运行状态变化后增加。

读取配置必须显式指定 Profile ID，不能依赖一个可在任务中变化的隐式当前 Profile。

### 10.2 激活 Profile

```text
CONFIG_ACTIVATE_PROFILE(profile_id)
    → active_profile
    → device_state_revision
```

切换完成后发送：

```text
CONFIG_ACTIVE_PROFILE_CHANGED {
    profile_id,
    device_state_revision,
    reason
}
```

Profile 切换不会破坏已经创建的不可变读快照。上位机收到事件后可以关闭旧快照并读取新的 Profile。

### 10.3 配置内容变化

配置对象 Commit 成功后发送：

```text
CONFIG_PROFILE_CHANGED {
    profile_id,
    profile_revision
}
```

如果被修改的 Profile 当前处于激活状态，固件在 Commit 后一次性应用新配置，再发送事件。上位机不需要在保存过程中响应中间读取通知。

## 11. 配置文档格式

配置对象使用版本化 Section 文档，不直接暴露整个运行时 C 对象图。

### 11.1 文档头

```c
typedef struct
{
    uint32_t magic;
    uint16_t schema_version;
    uint16_t profile_id;
    uint32_t section_count;
} __PACKED AmpConfigDocumentHeader;
```

建议 `magic` 使用 ASCII `AMPC` 的小端表示。

### 11.2 Section 头

```c
typedef struct
{
    uint16_t section_type;
    uint16_t section_version;
    uint32_t section_length;
} __PACKED AmpConfigSectionHeader;
```

Section 内容紧随其后。解析器遇到未知 Section 时按照 `section_length` 跳过。

建议的 Section：

```c
AMP_CONFIG_SECTION_GENERAL
AMP_CONFIG_SECTION_ADVANCED_KEYS
AMP_CONFIG_SECTION_KEYMAP
AMP_CONFIG_SECTION_RGB
AMP_CONFIG_SECTION_DYNAMIC_KEYS
AMP_CONFIG_SECTION_MACROS
AMP_CONFIG_SECTION_SCRIPT
AMP_CONFIG_SECTION_USER_BASE
```

### 11.3 定长记录数组

定长数组 Section 使用：

```c
typedef struct
{
    uint16_t record_version;
    uint16_t record_size;
    uint32_t record_count;
} __PACKED AmpRecordArrayHeader;
```

新版本可以在记录尾部增加字段。当前固件和配置器解析器会读取已知前缀，并根据
`record_size` 跳过未知尾部。未来若增加必需字段，应提高 Section Version；若新解析器
需要接受更短的旧记录，则由该版本解析器为缺失尾部字段提供默认值。

### 11.4 DynamicKey

DynamicKey 使用独立的变长记录，不把运行时 union 直接作为整个 Section ABI：

```c
typedef struct
{
    uint16_t key_index;
    uint16_t dynamic_type;
    uint16_t record_version;
    uint16_t payload_len;
} __PACKED AmpDynamicKeyRecordHeader;
```

记录后跟随对应类型的 Payload。未知 `dynamic_type` 或更高记录版本可以根据 `payload_len` 跳过。

对于明确版本且布局完全一致的 Wire Struct，固件仍然可以使用 `memcpy`，但必须：

- 仅包含固定宽度整数或明确定义的定点数；
- 不包含指针；
- 不依赖 C enum 大小；
- 使用小端序；
- 使用静态大小与偏移断言；
- 与运行时结构是否相同由实现决定，不能默认视作相同 ABI。

## 12. Debug、Console 与 Telemetry

### 12.1 订阅

流式数据必须显式订阅：

```text
DEBUG_SUBSCRIBE(options)
DEBUG_UNSUBSCRIBE
CONSOLE_SUBSCRIBE(options)
CONSOLE_UNSUBSCRIBE
TELEMETRY_SUBSCRIBE(options)
TELEMETRY_UNSUBSCRIBE
```

订阅属于 Session，新 HELLO 或断开后自动清理。

### 12.2 事件

流数据通过 `EVENT` 发送，`request_id == 0`。事件必须携带单调递增的流序号，使上位机能够检测丢帧，但 Stream Queue 不要求重传。

Console 和 Debug 不能使用 Response Queue，也不能阻塞配置读取、写入或 HELLO。

## 13. 必要校验

AMP 消息层只执行协议安全所必需的校验：

1. `proto == 0x41`；
2. 支持的 `version`；
3. 实际消息长度等于 `12 + payload_len`，或 Raw HID Report 至少包含这些字节；
4. `payload_len` 不超过当前 Transport 协商上限；
5. Flags 组合有效；
6. Session 有效；
7. Request ID 规则有效；
8. Opcode 存在；
9. Payload 至少包含对应 Opcode 的最小结构；
10. Object 事务 ID、offset、长度和 Revision 有效；声明 Object CRC32 能力时 CRC
    也必须有效。

业务代码不需要对固定 USB 填充区进行重复校验。Transport 错误、协议边界错误和业务参数错误应分层处理。

## 14. 典型流程

### 14.1 连接

```text
Host                                      Device
  |                                          |
  | HELLO(session=0x1234)                    |
  |----------------------------------------->|
  |                         reset old session|
  |<-----------------------------------------|
  | HELLO response, capabilities, profile    |
```

### 14.2 读取配置

```text
Host                                      Device
  | OBJECT_OPEN_READ(CONFIG, profile=3)      |
  |----------------------------------------->|
  |<-----------------------------------------|
  | transaction, revision, size, crc/0       |
  |                                          |
  | OBJECT_READ(transaction, offset, length) |
  |----------------------------------------->|
  |<-----------------------------------------|
  | data                                     |
  |                 ...                      |
  | OBJECT_CLOSE_READ(transaction)           |
  |----------------------------------------->|
```

一次读取始终来自同一个快照。Profile 切换事件不会让已返回的数据成为混合配置。

### 14.3 保存配置

```text
Host                                      Device
  | OBJECT_OPEN_WRITE(CONFIG, revision, ...) |
  |----------------------------------------->|
  |<-----------------------------------------|
  | transaction                              |
  |                                          |
  | OBJECT_WRITE(transaction, offset, data)  |
  |----------------------------------------->|
  |<-----------------------------------------|
  | OK                                       |
  |                 ...                      |
  | OBJECT_COMMIT(transaction)               |
  |----------------------------------------->|
  |                         verify and commit|
  |<-----------------------------------------|
  | new_revision                             |
  |<-----------------------------------------|
  | CONFIG_PROFILE_CHANGED event             |
```

如果页面在 Commit 前关闭，新会话会 Abort 未完成事务。声明
`AMP_CAP_OBJECT_ATOMIC_COMMIT` 时旧配置保持不变；直接写最终对象的设备不能提供该保证。

## 15. 实现边界

### 15.1 固件

固件建议拆分为：

- `amp_codec`：Header 编解码；
- `amp_session`：HELLO、Session、Request 去重；
- `amp_dispatch`：Channel/Opcode 分发；
- `amp_transport_*`：HID、UART 等适配器；
- `amp_object`：对象事务；
- `amp_config_document`：配置文档序列化；
- `amp_stream`：Debug、Console、Telemetry 订阅；
- `amp_queue`：Response、Control、Stream 队列。

USB 模板只负责 Transport Adapter，不再直接依赖 Packet 业务处理函数。

### 15.2 上位机

上位机建议拆分为：

- Transport：WebHID/WebUSB/Serial；
- Codec：AmpHeader 和 Payload；
- Session：HELLO、重连；
- Request Manager：Request ID、超时、重试；
- Object Client：Chunk 传输与 Commit；
- Config Codec：Section 文档；
- Controller：面向 UI 的高层接口。

UI 不再管理协议级读写重试。Controller 只在完整读取成功后发布新配置，只在 Commit 成功后报告保存成功。

## 16. 实现映射

当前实现已一次性切换到 v4，不维护双协议分支：

1. 固化本文 Header、Status、Channel 和 Opcode 表。
2. 为 C 和 TypeScript 生成共享协议常量及 Wire Struct 定义。
3. 实现 AMP Codec 和 Raw HID Adapter。
4. 实现 HELLO、Session Reset 和请求响应管理。
5. 实现三个优先级队列。
6. 实现 Object Service，以及可选的 CRC32 和文件系统临时对象原子提交。
7. 定义 Config Document Section 和编解码器。
8. 将配置读取、保存和脚本传输迁移至 Object Service。
9. 迁移 Debug 和 Console 订阅；Telemetry 仅保留 Channel，尚未定义业务 Opcode。
10. 重写上位机 `LibampKeyboardController`。
11. 删除旧 Packet GET/SET、旧 LargeTransfer 和 Version GET 会话启发式。
12. Raw HID、恢复和事务测试已加入；USB Bulk、UART、BLE、TCP Adapter 及其跨传输
    向量测试留待对应 Transport 实现时加入。

## 17. 测试要求

至少覆盖以下测试：

- Header 的 0 字节、最大 Payload 和非法长度；
- 所有请求响应字段回显；
- Request ID 回绕和重复请求；
- 新 HELLO 清理旧队列与旧订阅；
- WebHID 页面关闭时 Raw IN Busy 的恢复；
- Debug/Console 洪泛时控制响应仍可发送；
- Profile 在读取中切换时快照保持一致；
- 原子提交模式下写入中断后旧配置保持不变；
- Commit 重复请求不重复提交；
- Revision 冲突返回 `STALE_REVISION`；
- CRC32 模式下的错误 CRC，以及缺失、乱序和越界 Chunk；
- 未知 Config Section 和未知 DynamicKey 记录跳过；
- Raw HID、USB Bulk、UART Adapter 的同一组协议向量；
- 固件和 TypeScript 编解码黄金向量一致。

## 18. 设计结论

AMP vNext 不应只是给当前 6 字节 Header 增加一个长度字段。合理的重构边界是：

```text
12 字节版本化消息头
+ 显式 Session 与 Request ID
+ 传输适配层
+ 事务化 Object Service
+ Profile/Object Revision
+ 版本化配置 Section
+ 独立流式 QoS
```

该设计保留 Raw HID 的简单实现，同时不再把 AMP 的语义绑定到 64 字节 USB Report，并从协议层解决重新连接、配置一致性、LargeTransfer 恢复和结构扩展问题。
