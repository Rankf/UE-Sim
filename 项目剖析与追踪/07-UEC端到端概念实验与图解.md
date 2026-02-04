# UEC 端到端概念实验与图解

本文档提供覆盖 **Ultra Ethernet Consortium (UEC)** 所有关键概念的端到端实验说明，并辅以详细图解，便于理解 Soft-UE ns-3 项目。

---

## 一、UEC 关键概念速查表

| 缩写 | 全称 | 中文 | 作用 |
|------|------|------|------|
| **SES** | Semantic Sub-layer | 语义子层 | 操作类型识别、端点寻址、授权验证、元数据管理 |
| **PDS** | Packet Delivery Sub-layer | 包分发子层 | 包分类路由、PDC 管理分配、拥塞控制、流量管理 |
| **PDC** | Packet Delivery Context | 包分发上下文 | 单次传输的状态与通道（可靠/不可靠） |
| **IPDC** | Immediate PDC | 不可靠 PDC | 无确认、低延迟，适合对丢包不敏感流量 |
| **TPDC** | Transactional PDC | 可靠 PDC | 有确认与 RTO 重传，保证可靠交付 |
| **FEP** | Fabric Endpoint | 网络端点 | 节点在网络中的唯一标识（如 FEP=1, FEP=2） |
| **MSN** | Message Sequence Number | 消息序列号 | 用于乱序重组与消息边界（SOM/EOM） |
| **RTO** | Retransmission Timeout | 重传超时 | TPDC 下未收到 ACK 时的重传计时 |
| **SOM/EOM** | Start/End of Message | 消息起止 | PDS 头中标记消息开始与结束的包 |

---

## 二、协议栈与数据流总览

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     UEC 端到端数据流（单包视角）                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  发送端 (Node 0, FEP=1)                        接收端 (Node 1, FEP=2)       │
│                                                                             │
│  ┌──────────────────┐                        ┌──────────────────┐         │
│  │ ① 应用层          │                        │ ⑧ 应用层          │         │
│  │ SendPacket()     │                        │ HandleRead()     │         │
│  └────────┬─────────┘                        └────────▲─────────┘         │
│           │ ② 构造 OperationMetadata                 │                     │
│           │    (OpType, FEP, job_id, messages_id)     │                     │
│           ▼                                           │                     │
│  ┌──────────────────┐                        ┌───────┴─────────┐         │
│  │ ③ SES 层          │                        │ ⑦ SES 层         │         │
│  │ ProcessSendRequest│                        │ 校验/MSN 更新    │         │
│  │ 验证端点/授权     │                        │                  │         │
│  └────────┬─────────┘                        └───────▲─────────┘         │
│           │ ④ 构造 PDSHeader                          │                     │
│           │    (pdc_id, seq_num, SOM, EOM)            │                     │
│           ▼                                           │                     │
│  ┌──────────────────┐                        ┌───────┴─────────┐         │
│  │ ⑤ PDS 层          │                        │ ⑥ PDS 层         │         │
│  │ 选 PDC/路由       │                        │ 解析头/投递 PDC   │         │
│  └────────┬─────────┘                        └───────▲─────────┘         │
│           │                                          │                     │
│           ▼                                          │                     │
│  ┌──────────────────┐     SoftUeChannel     ┌───────┴─────────┐         │
│  │ SoftUeNetDevice  │ ────────────────────▶ │ SoftUeNetDevice  │         │
│  │ (FEP=1)          │     (延迟/带宽)        │ (FEP=2)          │         │
│  └──────────────────┘                        └──────────────────┘         │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 三、各层概念图解

### 3.1 FEP（Fabric Endpoint）— 谁和谁通信

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        FEP：网络端点标识                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Node 0                     SoftUeChannel                  Node 1      │
│   ┌─────────────┐                                              ┌───────┴─────┐
│   │ SoftUe      │   FEP 由 Helper 在 Install 时分配：           │ SoftUe      │
│   │ NetDevice   │   config.localFep = (MAC 低 16 位) 或 i+1     │ NetDevice   │
│   │ FEP = 1     │ ────────────────────────────────────────────▶ │ FEP = 2     │
│   └─────────────┘   寻址：目标 FEP=2 决定包被哪个设备接收       └─────────────┘
│                                                                         │
│   OperationMetadata 中：                                                │
│   • SetSourceEndpoint(nodeId, endpointId)  → 源 FEP 侧信息              │
│   • SetDestinationEndpoint(nodeId, endpointId) → 目标 FEP 侧信息        │
│   实际路由用 nodeId 映射到设备 FEP（如 node 0 → FEP 1, node 1 → FEP 2）  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 3.2 SES（语义子层）— 操作类型与元数据

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        SES：语义子层                                    │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │              ExtendedOperationMetadata                         │   │
│   ├─────────────────────────────────────────────────────────────────┤   │
│   │  op_type        │ SEND / READ / WRITE / DEFERRABLE              │   │
│   │  s_pid_on_fep   │ 源端点上的进程 ID                              │   │
│   │  t_pid_on_fep   │ 目标端点上的进程 ID                            │   │
│   │  job_id         │ 作业 ID（多任务隔离）                           │   │
│   │  messages_id    │ 消息 ID（对应 MSN，用于重组）                   │   │
│   │  payload        │ start_addr, length, imm_data                    │   │
│   │  use_optimized_header / has_imm_data                             │   │
│   └─────────────────────────────────────────────────────────────────┘   │
│                                    │                                    │
│                                    ▼                                    │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │  SesManager::ProcessSendRequest(metadata)                      │   │
│   │  • 校验源/目标端点                                             │   │
│   │  • 与 PdsManager 协作完成发送路径                               │   │
│   │  • MSN 表维护（接收侧乱序重组）                                 │   │
│   └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 3.3 PDS（包分发子层）— 包头与 PDC 选择

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        PDS：包分发子层与 PDSHeader                      │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │                      PDSHeader (每个包)                          │   │
│   ├──────────┬──────────┬──────────┬──────────┐                     │   │
│   │ pdc_id   │ seq_num  │   SOM    │   EOM    │                     │   │
│   │ (选哪条  │ (序号，  │ (消息    │ (消息    │                     │   │
│   │  PDC)    │  MSN 相关)│  首包=1) │  末包=1) │                     │   │
│   └──────────┴──────────┴──────────┴──────────┘                     │   │
│                                                                         │
│   PDC 分配（PdcAllocator）：                                            │
│   • IPDC: pdc_id ∈ [1, 512]   → 不可靠、低延迟                         │
│   • TPDC: pdc_id ∈ [513, 1024]→ 可靠、有 RTO 重传                       │
│                                                                         │
│   发送路径： 应用 → SES → 填 PDSHeader(pdc_id, seq, SOM, EOM) → PDS 路由 │
│              → 对应 PDC → SoftUeNetDevice → Channel                    │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 3.4 PDC（IPDC vs TPDC）— 可靠与不可靠

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        PDC：传输上下文（IPDC / TPDC）                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   ┌─────────────────────────────┐  ┌─────────────────────────────┐     │
│   │         IPDC               │  │         TPDC                 │     │
│   │  (Immediate / 不可靠)      │  │  (Transactional / 可靠)      │     │
│   ├─────────────────────────────┤  ├─────────────────────────────┤     │
│   │ • pdc_id: 1..512            │  │ • pdc_id: 513..1024          │     │
│   │ • 无 ACK                     │  │ • 有 ACK/NACK                │     │
│   │ • 无重传                     │  │ • RtoTimer 超时重传          │     │
│   │ • 低延迟、可丢包             │  │ • 保证按序/可靠交付          │     │
│   └─────────────────────────────┘  └─────────────────────────────┘     │
│                                                                         │
│   本项目中：压力测试多用 IPDC 段内 pdc_id（如 1..maxPdcCount）以追求速率； │
│   若需可靠语义，则分配 TPDC 并依赖 RTO 与重传逻辑。                      │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 3.5 MSN 与 SOM/EOM — 消息边界

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        MSN 与 消息边界 (SOM/EOM)                         │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   一条“消息”可能被拆成多个包：                                           │
│                                                                         │
│   Packet 0:  [PDSHeader: pdc_id=1, seq=1, SOM=1, EOM=0]  ← 消息开始     │
│   Packet 1:  [PDSHeader: pdc_id=1, seq=2, SOM=0, EOM=0]                 │
│   Packet 2:  [PDSHeader: pdc_id=1, seq=3, SOM=0, EOM=1]  ← 消息结束     │
│                                                                         │
│   • messages_id (SES 元数据) 与 seq_num (PDS) 共同标识顺序与边界。      │
│   • 接收侧 MSN 表（MsnEntry）：last_psn, expected_len, pdc_id，用于     │
│     乱序重组与判断消息是否完整。                                         │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 3.6 RTO（重传超时）— 仅 TPDC

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        RTO：重传超时（TPDC 专用）                        │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   TPDC 发送包后：                                                        │
│                                                                         │
│   ┌──────────────┐    未收到 ACK      ┌──────────────┐                  │
│   │ 发送数据包   │ ─────────────────▶ │ RtoTimer     │                  │
│   └──────────────┘    超时            │ 到期         │                  │
│         │                             └──────┬───────┘                  │
│         │                                    │ 触发重传                  │
│         │                                    ▼                          │
│         │                             ┌──────────────┐                  │
│         └─────────────────────────────│ 重传该包     │                  │
│                                       └──────────────┘                  │
│                                                                         │
│   transport-layer.h: Base_RTO, RTO_Init_Time, Max_RTO_Retx_Cnt 等       │
│   rto-timer/ 模块实现 TPDC 侧计时与重传。                                │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 四、端到端实验脚本说明

### 4.1 实验位置与运行方式

| 项目 | 说明 |
|------|------|
| **脚本路径** | `scratch/Soft-UE-E2E-Concepts/uec-e2e-concepts.cc` |
| **编译** | `./ns3 build`（与主项目一起编译） |
| **运行** | `./ns3 run uec-e2e-concepts`（短名称匹配） |
| **端到端流程日志** | 默认已开启，运行后可见所有 `[UEC-E2E]` 开头的关键节点日志（①②③… 对应下图解） |
| **仅看 E2E 日志** | `grep UEC-E2E` 过滤，或关闭其他：`NS_LOG="UecE2EConcepts=info:SesManager=info:SoftUeNetDevice=info:SoftUeChannel=info" ./ns3 run uec-e2e-concepts` |

若出现 **`Couldn't find the specified program: uec-e2e-concepts`**，说明 ns3 的节目录未包含该可执行文件，需先重新配置再编译：

```bash
./ns3 configure --enable-examples --enable-tests
./ns3 build
./ns3 run uec-e2e-concepts
```

或直接运行生成的可执行文件：

```bash
./build/scratch/Soft-UE-E2E-Concepts/ns3.44-uec-e2e-concepts-debug
```

### 4.1.1 端到端流程关键日志（[UEC-E2E]）

运行 `./ns3 run uec-e2e-concepts` 后，每个包会按顺序打印下列关键节点，便于对照「二、协议栈与数据流总览」理解全流程：

| 步骤 | 日志前缀 | 含义 |
|------|----------|------|
| ① | `[App] ① 应用层 构造包` | 应用创建包、size、seq |
| ② | `[App] ② PDS 头` | 填 pdc_id, seq, SOM, EOM |
| ③ | `[App] ③ SES 元数据` | 填 src/dst、job_id、messages_id |
| ③ | `[SES] ③ SES 层 ProcessSendRequest` | SES 校验通过，允许发送 |
| ④ | `[App] ④ 打时间戳 → device->Send()` | 打时间戳并交给设备 |
| ⑤ | `[Device] ⑤ 设备层 Send` | 设备发往信道（FEP→FEP） |
| ⑤ | `[Channel] ⑤ 信道 Transmit` | 信道转发（经延迟） |
| ⑥ | `[Channel] ⑥ 信道 ReceivePacket` | 信道送达对端设备 |
| ⑥ | `[Device] ⑥ 设备层 ReceivePacket` | 设备收包入队 |
| ⑦ | `[Device] ⑦ 设备层 ProcessReceiveQueue` | 设备递交应用层 |
| ⑧ | `[App] ⑧ 应用层 收包` | 应用 HandleRead 解析 PDS 头 |

### 4.1.2 如何运行与查看

- **直接运行（默认会打 E2E 日志）**
  ```bash
  ./ns3 run uec-e2e-concepts
  ```

- **只看端到端节点**
  ```bash
  ./ns3 run uec-e2e-concepts 2>&1 | grep UEC-E2E
  ```

- **只看第一个包的完整流程**
  ```bash
  ./ns3 run uec-e2e-concepts 2>&1 | grep -A 100 "seq=1 " | head -15
  ```

### 4.1.3 一个包的完整流程说明（对照日志理解）

下面以 **第一个包（seq=1）** 为例，按时间顺序说明从“应用构造包”到“对端应用收包”的整条路径。每条对应你运行上述命令时看到的一行 `[UEC-E2E]` 日志。

---

**发送端（Node 0，FEP=4）**

| 步骤 | 日志示例 | 含义 |
|------|----------|------|
| **① 应用层 构造包** | `[App] ① 应用层 构造包 size=256 seq=1` | 应用创建空包（payload 256 字节），并决定这是第 1 个包（seq=1）。此时还没有任何协议头。 |
| **② PDS 头** | `[App] ② PDS 头 pdc_id=1 seq=1 SOM=true EOM=false` | 应用按 UEC 规范填 **PDS 头**：选一条“通道”`pdc_id=1`（IPDC 段）、序号 `seq=1`、消息边界 `SOM=首包, EOM=非末包`。包从 256 变成 256+头长。 |
| **③ SES 元数据** | `[App] ③ SES 元数据 src_node=1 dst_node=2 job_id=12345 messages_id=1` | 应用填 **语义层** 信息：谁发给谁（src_node=1, dst_node=2）、作业 ID、消息 ID（对应 MSN）。这些不放在包头上，而是交给 SES 做校验和后续处理。 |
| **③ SES 校验** | `[SES] ③ SES 层 ProcessSendRequest: ... → 校验通过，允许发送` | **SES 层** 根据元数据做端点/权限等校验，通过后允许本包继续往下发。 |
| **④ 打时间戳** | `[App] ④ 打时间戳 → 调用 device->Send()` | 应用在包上打 **发送时间戳**（供对端算延迟），然后调用 **device->Send()** 把包交给本节点的网络设备。 |
| **⑤ 设备发往信道** | `[Device] ⑤ 设备层 Send: FEP 4 → FEP 8 size=263 B` | **SoftUeNetDevice** 根据目标地址解析出目标 FEP=8，把包交给 **SoftUeChannel**（信道）。263 B = 256 负载 + PDS 头等。 |
| **⑤ 信道转发** | `[Channel] ⑤ 信道 Transmit: FEP 4 → FEP 8 size=263 B (经延迟后送达对端)` | **信道** 收到包后，按配置的 **延迟 + 传输时间** 在仿真里安排“在将来某时刻送达 FEP=8 所在设备”。 |

---

**接收端（Node 1，FEP=8）**

| 步骤 | 日志示例 | 含义 |
|------|----------|------|
| **⑥ 信道送达设备** | `[Channel] ⑥ 信道 ReceivePacket: FEP 4 → FEP 8 送达设备，设备即将 ReceivePacket` | 仿真时间到达后，信道触发“包到达 FEP=8 的设备”，即调用对端 **SoftUeNetDevice::ReceivePacket**。 |
| **⑥ 设备收包入队** | `[Device] ⑥ 设备层 ReceivePacket: FEP 4 → FEP 8 size=263 B → 入队，待递交应用层` | 设备确认包是发给本机 FEP 的，更新统计（含用时间戳算延迟），把包放入 **接收队列**，等待交给上层。 |
| **⑦ 设备递交应用** | `[Device] ⑦ 设备层 ProcessReceiveQueue: 递交应用层 HandleRead` | 设备从队列里取出包，调用应用注册的 **接收回调**，即应用层的 **HandleRead**。 |
| **⑧ 应用层收包** | `[App] ⑧ 应用层 收包 seq=1 pdc_id=1 size=263 (累计接收 1 包)` | 应用在 **HandleRead** 里解析 **PDS 头**（得到 seq=1, pdc_id=1），做统计或业务处理。至此“一个包”的端到端流程结束。 |

---

**小结（一句话串起来）**

一个包从 **应用构造（①）→ 填 PDS 头（②）→ 填 SES 元数据并经 SES 校验（③）→ 打时间戳并交给设备（④）→ 设备发往信道（⑤）→ 信道经延迟后送达对端设备（⑥）→ 对端设备入队并递交应用（⑦）→ 对端应用 HandleRead 解析 PDS 头（⑧）**。FEP 用来在信道/设备层区分“发给谁”（FEP=8 即 Node 1）；PDS 头里的 pdc_id/seq/SOM/EOM 用来在做多包、多流时区分通道和消息边界。

### 4.2 实验覆盖的概念与顺序

```
┌─────────────────────────────────────────────────────────────────────────┐
│                     uec-e2e-concepts 实验阶段                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Phase 0: 拓扑与 FEP                                                    │
│  • 创建 2 节点，SoftUeHelper::Install → 每节点 SoftUeNetDevice          │
│  • 打印各节点 FEP（Fabric Endpoint），理解“端点”是谁                      │
│                                                                         │
│  Phase 1: SES — OperationMetadata 与 ProcessSendRequest                  │
│  • 构造 ExtendedOperationMetadata（OpType::SEND, job_id, messages_id,   │
│    SetSourceEndpoint / SetDestinationEndpoint）                          │
│  • 调用 SesManager::ProcessSendRequest，确认 SES 参与发送路径            │
│                                                                         │
│  Phase 2: PDS — PDSHeader 与 PDC 分配                                   │
│  • 为每个包设置 PDSHeader：pdc_id（IPDC 段）、seq_num、SOM、EOM          │
│  • 理解 pdc_id 与 IPDC/TPDC 区间（1..512 vs 513..1024）                 │
│                                                                         │
│  Phase 3: 完整发送与接收                                                 │
│  • 客户端按间隔发送 N 个包，服务端 ReceiveCallback 收包                  │
│  • 收包后解析 PDSHeader，统计 SES/PDS 处理次数与延迟                     │
│                                                                         │
│  Phase 4: 概念清单与统计                                                │
│  • 打印“本实验覆盖的 UEC 概念”清单（FEP, SES, PDS, PDC, IPDC, TPDC,      │
│    MSN, SOM/EOM, RTO）                                                   │
│  • 输出 PdsStatistics、收发包数、吞吐与延迟                             │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 4.3 与主压力测试的对比

| 对比项 | scratch/Soft-UE/Soft-UE.cc | scratch/Soft-UE-E2E-Concepts/uec-e2e-concepts.cc |
|--------|----------------------------|--------------------------------------------------|
| 目标 | 200Gbps 压力与性能统计 | 理解 UEC 概念、覆盖全栈与图解对应 |
| 包数量 | 10 万级 | 少量（如 20）便于观察 |
| 输出 | CSV、延迟分布、吞吐 | 分阶段打印 FEP/SES/PDS/PDC 与概念清单 |
| 图解 | 无 | 与本文档 07 图解一一对应 |

---

## 五、单包生命周期（与图解对应）

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    单包生命周期（与第二节图解对应）                       │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ① 应用层：Create<Packet>, 决定大小与数量                               │
│  ② 应用层：构造 ExtendedOperationMetadata（FEP、OpType、job_id、        │
│            messages_id）→ 对应 §3.1 FEP、§3.2 SES                        │
│  ③ SES：  ProcessSendRequest(metadata) → §3.2                           │
│  ④ 应用层：添加 PDSHeader(pdc_id, seq_num, SOM, EOM) → §3.3 PDS         │
│  ⑤ PDS：  设备 Send 时按 pdc_id 走对应 PDC（IPDC/TPDC）→ §3.4          │
│  ⑥ 接收端：SoftUeNetDevice 收包 → PDS 解析头 → 投递到应用                │
│  ⑦ 接收端：SES 侧可更新 MSN/乱序重组 → §3.5                             │
│  ⑧ 应用层：HandleRead 解析 PDSHeader，统计延迟与计数                     │
│                                                                         │
│  若使用 TPDC：发送后未收到 ACK 会由 RtoTimer 触发重传 → §3.6            │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 六、与 UEC 参考图对照及欠缺点（基于代码）

**说明**：UEC 分层图（Initiator/Target）中的 **虚线表示逻辑路径**（如 Transactions、Messages、UET Packets），不是物理线缆路径；物理传输在 MAC 层（图中实线）。下面对照 **Core Components 图（SES–PDS Manager–PDC）** 与当前代码，逐条列出欠缺点。

### 6.1 Core Components 图期望的交互（简述）

- **SES（左）→ PDS Manager**：控制面 Tx req、Eager req（发送请求）
- **PDS Manager → PDCs**：管理/控制 Tx req、Rx req、Rx rsp、Rx cm、Close
- **数据路径**：数据应**水平穿过 PDC**（左接口 → PDC → 右接口），即由 PDS Manager 分配 PDC，经 **SendPacketThroughPdc** 发/收
- **SES（左）← PDC**：数据/状态面 Tx rsp（发送响应）
- **SES（右）↔ PDS Manager**：Rx req、Rx rsp、Rx cm、Tx nack 等
- **PDC → SES（右）**：Tx req、Tx rsp、Tx cm（收端经 PDC 上报）

### 6.2 基于代码的欠缺点分析

| 欠缺点 | 图中期望 | 当前代码 | 代码位置 |
|--------|----------|----------|----------|
| **1. SES 未向 PDS Manager 提交 Tx req** | SES 发 Tx req（发送请求）给 PDS Manager | SesManager::ProcessSendRequest 里**构造了 SesPdsRequest**（等价于 Tx req），但**没有调用** m_pdsManager->ProcessSesRequest(sesRequest) 或 DispatchPacket(sesRequest)；注释写 "packet is sent directly by application through device->Send(), so we don't need to create PDS requests" | ses-manager.cc 约 159–189 行 |
| **2. 数据未经过 PDC 发送** | 数据应经 PDC（PDS Manager 分配 PDC 后 SendPacketThroughPdc） | SoftUeNetDevice::Send 直接 **channel->Transmit**，未调用 PdsManager::ProcessSesRequest 或 SendPacketThroughPdc；应用只把 pdc_id 写在 PDS 头里，由设备直发 | soft-ue-net-device.cc 约 305–316 行（"Send directly through channel (bypass PDS manager for now)"） |
| **3. PDS Manager 的 ProcessSesRequest 也未经 PDC** | PDS Manager 收到请求后应经 PDC 发（AllocatePdc + SendPacketThroughPdc） | PdsManager::ProcessSesRequest 收到 SesPdsRequest 后**直接** channel->Transmit(request.packet, ...)，**没有**调用 AllocatePdc 或 SendPacketThroughPdc；真正“经 PDC 发”的路径是 **DispatchPacket**（AllocatePdc + SendPacketThroughPdc），但 E2E 中无人调用 DispatchPacket | pds-manager.cc ProcessSesRequest 约 174–184 行；DispatchPacket 约 447–472 行 |
| **4. 接收路径未经过 PDS/PDC** | 对端收包应经 PDS Manager/PDC 再交给 SES（Rx req/Rx rsp 等） | 设备 ReceivePacket 后入队、ProcessReceiveQueue 直接调应用 HandleRead；**没有**调用 PdsManager::ProcessReceivedPacket，也没有“收包经 PDC 再交给 SES”的 ProcessReceiveRequest | soft-ue-net-device.cc ReceivePacket、ProcessReceiveQueue |
| **5. 无 Tx rsp / Eager size / Pause / Error event 等控制面** | 图中 SES 从 PDS Manager 收 Eager size、Pause、Error event；从 PDC 收 Tx rsp | 当前发送是同步 device->Send()，无“经 PDC 发完后回调 SES”的 Tx rsp 或上述信令 | 无对应实现 |
| **6. 无 Rx req / Rx rsp / Rx cm（接收侧 SES ↔ PDS Manager）** | 图中右端 SES 与 PDS Manager 有 Rx req、Rx rsp、Rx cm | 接收侧未走 PDS Manager，也未向 SES 发 ProcessReceiveRequest | 无对应实现 |

### 6.3 小结与改进方向

- **核心欠缺点**：**PDC 承载数据路径未走 PDC 发送**。即 PDC/IPDC/TPDC 概念和 pdc_id 在头里都有，但当前 E2E **未通过 AllocatePdc + SendPacketThroughPdc 分配 PDC 再发**，只是把 pdc_id 写在 PDS 头里由设备直发；接收侧也未经 PDS Manager/PDC。
- **改进方向**（若要与 Core Components 图一致）：
  1. **发送**：SES::ProcessSendRequest 在校验通过后，将 SesPdsRequest 交给 PdsManager（如 ProcessSesRequest 或 DispatchPacket）；PdsManager 应经 **AllocatePdc + SendPacketThroughPdc** 发，而不是直接 channel->Transmit。或设备 Send 时根据包/元数据调用 PdsManager::DispatchPacket / SendPacketThroughPdc，使数据路径经过 PDC。
  2. **PdsManager::ProcessSesRequest**：若保留该接口，应改为内部调用 DispatchPacket（AllocatePdc + SendPacketThroughPdc），使“SES 请求”统一经 PDC 发出。
  3. **接收**：设备收包后先交给 PdsManager::ProcessReceivedPacket，再由 PDS/PDC 层按 pdc_id 分发并视需要调用 SesManager::ProcessReceiveRequest，形成“经 PDC 收、再交 SES”的路径。
  4. **控制面**：视需要增加 Tx rsp、Eager size、Pause、Error event、Rx req/Rx rsp 等信令的占位或简化实现。

---

## 七、小结

- **01-项目总览图解** 给出协议栈与目录映射；**本实验与图解** 把 UEC 的 FEP、SES、PDS、PDC、IPDC、TPDC、MSN、SOM/EOM、RTO 与真实代码路径、实验阶段对应起来。
- 运行 `./ns3 run uec-e2e-concepts` 并对照本文档中的图解，可逐概念理解 Soft-UE ns-3 中的 UEC 实现。
- **六** 中对照 Core Components 图给出了基于代码的欠缺点与改进方向，便于后续让数据路径真正经 PDC 收发。
