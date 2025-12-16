# Soft-UE ns-3 组件关系可视化

## 目录结构
```
docs/
├── migration-analysis-report.md    # 迁移对比分析报告
├── soft-ue-architecture.svg        # 架构总览图
├── soft-ue-dataflow.svg            # 数据流程图
└── component-diagram.md            # 本文档
```

---

## 1. 协议栈层次图 (文字版)

```
┌─────────────────────────────────────────────────────────────────────┐
│                         APPLICATION LAYER                            │
│  ┌─────────────────────────────────────────────────────────────────┐│
│  │              SoftUeApplication (Client/Server)                   ││
│  │         • SendPacket() / HandlePacket()                          ││
│  │         • 统计计数: PacketsSent, PacketsReceived                 ││
│  └─────────────────────────────────────────────────────────────────┘│
└──────────────────────────────┬──────────────────────────────────────┘
                               │ ExtendedOperationMetadata
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                          SES LAYER                                   │
│  ┌─────────────────────────────────────────────────────────────────┐│
│  │                        SesManager                                ││
│  │   ┌────────────────┐  ┌────────────────┐  ┌──────────────────┐  ││
│  │   │ MsnTable       │  │OperationMeta  │  │  Validation      │  ││
│  │   │ 消息序列号跟踪  │  │ 操作元数据     │  │  JobID/权限验证   │  ││
│  │   └────────────────┘  └────────────────┘  └──────────────────┘  ││
│  │   Functions:                                                     ││
│  │   • ProcessSendRequest()    - 处理发送请求                       ││
│  │   • ProcessReceiveRequest() - 处理接收请求                       ││
│  │   • ProcessReceiveResponse()- 处理响应                           ││
│  └─────────────────────────────────────────────────────────────────┘│
└──────────────────────────────┬──────────────────────────────────────┘
                               │ SesPdsRequest / PdcSesRequest
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                          PDS LAYER                                   │
│  ┌─────────────────────────────────────────────────────────────────┐│
│  │                        PdsManager                                ││
│  │   ┌────────────────┐  ┌────────────────┐  ┌──────────────────┐  ││
│  │   │ PDSHeader      │  │ PdsStatistics  │  │  PDC Pool        │  ││
│  │   │ 头部序列化     │  │ 性能统计       │  │  PDC分配管理      │  ││
│  │   └────────────────┘  └────────────────┘  └──────────────────┘  ││
│  │   Functions:                                                     ││
│  │   • ProcessSesRequest()     - 处理SES请求                        ││
│  │   • ProcessReceivedPacket() - 处理接收包                         ││
│  │   • AllocatePdc()           - 分配PDC                            ││
│  │   • ReleasePdc()            - 释放PDC                            ││
│  └─────────────────────────────────────────────────────────────────┘│
└──────────────────────────────┬──────────────────────────────────────┘
                               │ Ptr<Packet> + PDC Operations
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                          PDC LAYER                                   │
│  ┌─────────────────────────────┐  ┌────────────────────────────────┐│
│  │         PdcBase             │  │       PdcStatistics            ││
│  │  (ns3::Object 抽象基类)     │  │  • packetsSent/Received        ││
│  │  • GetPdcId() / SetPdcId()  │  │  • bytesTransmitted/Received   ││
│  │  • SendPacket() [纯虚]      │  │  • retransmissions/timeouts    ││
│  │  • HandleReceivedPacket()   │  │  • averageLatency              ││
│  └───────────────┬─────────────┘  └────────────────────────────────┘│
│                  │                                                   │
│       ┌──────────┴──────────┐                                        │
│       ▼                     ▼                                        │
│  ┌──────────────┐    ┌──────────────┐    ┌─────────────────────────┐│
│  │    IPDC      │    │    TPDC      │    │      RTOTimer           ││
│  │ 不可靠PDC    │    │  可靠PDC     │    │   超时重传计时器         ││
│  │              │    │              │    │                         ││
│  │ • UUD模式    │    │ • RUD/ROD    │    │ • StartPacketTimer()    ││
│  │ • 无确认     │    │ • ACK/NACK   │    │ • StopPacketTimer()     ││
│  │ • 无重传     │    │ • 重传机制   │    │ • CheckTimeout()        ││
│  └──────────────┘    └──────────────┘    └─────────────────────────┘│
└──────────────────────────────┬──────────────────────────────────────┘
                               │ Ptr<Packet>
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                       NETWORK DEVICE LAYER                           │
│  ┌─────────────────────────────────────────────────────────────────┐│
│  │                    SoftUeNetDevice                               ││
│  │   ┌────────────────┐  ┌────────────────┐  ┌──────────────────┐  ││
│  │   │ MAC Address    │  │ FEP (Fabric    │  │ SoftUeChannel    │  ││
│  │   │ 02-06-XX:XX    │  │ EndPoint)      │  │ 点对点通道       │  ││
│  │   └────────────────┘  └────────────────┘  └──────────────────┘  ││
│  │   Functions:                                                     ││
│  │   • Send()              - 发送到Channel                          ││
│  │   • Receive()           - 从Channel接收                          ││
│  │   • SetChannel()        - 绑定通道                               ││
│  │   • GetSesManager()     - 获取SES管理器                          ││
│  │   • GetPdsManager()     - 获取PDS管理器                          ││
│  └─────────────────────────────────────────────────────────────────┘│
└──────────────────────────────┬──────────────────────────────────────┘
                               │ Physical Transmission
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                         SOFT-UE CHANNEL                              │
│              点对点物理传输 (ns-3 Channel Model)                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 2. 类继承关系

```
ns3::Object
    │
    ├── SesManager
    │       └── 拥有 → MsnTable, OperationMetadata
    │
    ├── PdsManager
    │       ├── 拥有 → PdsStatistics
    │       └── 管理 → std::map<uint16_t, Ptr<PdcBase>> m_pdcs
    │
    ├── PdcBase (抽象基类)
    │       ├── IPDC (不可靠PDC实现)
    │       └── TPDC (可靠PDC实现)
    │               └── 拥有 → RTOTimer
    │
    ├── SoftUeNetDevice (继承 ns3::NetDevice)
    │       ├── 拥有 → Ptr<SesManager>
    │       ├── 拥有 → Ptr<PdsManager>
    │       └── 关联 → Ptr<SoftUeChannel>
    │
    └── SoftUeChannel (继承 ns3::Channel)
            └── 连接多个 SoftUeNetDevice
```

---

## 3. 数据结构流转

### 3.1 发送路径数据转换
```
ExtendedOperationMetadata  ──(SES)──→  SesPdsRequest
                                            │
                                           (PDS)
                                            │
                                            ▼
                                    PDSHeader + Packet
                                            │
                                           (PDC)
                                            │
                                            ▼
                                      ns3::Packet
                                            │
                                        (NetDevice)
                                            │
                                            ▼
                                       Channel传输
```

### 3.2 接收路径数据转换
```
Channel传输  ──(NetDevice)──→  ns3::Packet
                                    │
                                   (PDC)
                                    │
                                    ▼
                            Parsed PDSHeader
                                    │
                                   (PDS)
                                    │
                                    ▼
                             PdcSesRequest
                                    │
                                   (SES)
                                    │
                                    ▼
                        Application Callback
```

---

## 4. 关键接口调用图

```
┌─────────────────────────────────────────────────────────────────────┐
│ 发送流程 (Client)                                                    │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  App::SendPacket(data, size)                                        │
│       │                                                              │
│       ├─→ metadata = CreateMetadata()                                │
│       │                                                              │
│       └─→ sesManager->ProcessSendRequest(metadata)                   │
│                │                                                     │
│                ├─→ ValidateOperationMetadata()                       │
│                ├─→ InitializeSesHeader()                             │
│                ├─→ GenerateMessageId()                               │
│                │                                                     │
│                └─→ pdsManager->ProcessSesRequest(sesPdsReq)          │
│                         │                                            │
│                         ├─→ ValidateSesPdsRequest()                  │
│                         ├─→ AllocatePdc() → pdcId                    │
│                         │                                            │
│                         └─→ pdc->SendPacket(packet, som, eom)        │
│                                  │                                   │
│                                  ├─→ CreatePdsHeader()               │
│                                  ├─→ UpdateStatistics()              │
│                                  │                                   │
│                                  └─→ netDevice->Send(packet)         │
│                                           │                          │
│                                           └─→ channel->Transmit()    │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│ 接收流程 (Server)                                                    │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  channel->Deliver(packet) → netDevice                                │
│       │                                                              │
│       └─→ netDevice->Receive(packet, srcFep)                         │
│                │                                                     │
│                └─→ pdsManager->ProcessReceivedPacket(packet)         │
│                         │                                            │
│                         ├─→ ParsePdsHeader()                         │
│                         ├─→ GetPdc(pdcId) → pdc                      │
│                         │                                            │
│                         └─→ pdc->HandleReceivedPacket(packet)        │
│                                  │                                   │
│                                  ├─→ ValidatePacket()                │
│                                  ├─→ UpdateStatistics()              │
│                                  │                                   │
│                                  └─→ NotifyPdsManager()              │
│                                                                      │
│                         ◄────────────(回调返回PDS)──────────────────┘ │
│                         │                                            │
│                         └─→ sesManager->ProcessReceiveRequest()      │
│                                  │                                   │
│                                  ├─→ ParseReceivedRequest()          │
│                                  │                                   │
│                                  └─→ app->HandlePacket(callback)     │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 5. PDC状态机

```
                    ┌──────────────────┐
                    │      CLOSED      │
                    └────────┬─────────┘
                             │ Initialize()
                             ▼
                    ┌──────────────────┐
                    │     CREATING     │
                    └────────┬─────────┘
                             │ 握手完成
                             ▼
         ┌─────────────────────────────────────────┐
         │              ESTABLISHED                 │
         │    ┌────────────────────────────────┐   │
         │    │     正常数据传输状态            │   │
         │    │  • SendPacket()                │   │
         │    │  • HandleReceivedPacket()      │   │
         │    │  • 超时重传 (TPDC)             │   │
         │    └────────────────────────────────┘   │
         └──────────────────┬──────────────────────┘
                            │ Close请求
                            ▼
                    ┌──────────────────┐
                    │     QUIESCE      │────┐
                    │    等待清空队列   │    │ 等待ACK
                    └────────┬─────────┘    │
                             │              │
                             ▼              │
                    ┌──────────────────┐    │
                    │   ACK_WAIT       │◄───┘
                    └────────┬─────────┘
                             │ 收到最终ACK
                             ▼
                    ┌──────────────────┐
                    │      CLOSED      │
                    └──────────────────┘
```

---

## 6. 配置参数

| 参数 | 默认值 | 描述 |
|------|--------|------|
| MAX_PDC | 512 | 每种类型最大PDC数量 |
| MAX_MTU | 4096 | 最大传输单元 |
| Base_RTO | 100ms | 基础重传超时 |
| Max_RTO_Retx_Cnt | 5 | 最大重传次数 |
| Default_MPR | 8 | 默认最大待确认包数 |
| UDP_Dest_Port | 2887 | UET协议UDP端口 |

---

## 7. 性能指标收集点

```
┌─────────────────────────────────────────────────────────────────┐
│                     PdcStatistics                                │
├─────────────────────────────────────────────────────────────────┤
│ • packetsSent / packetsReceived     - 包计数                    │
│ • bytesTransmitted / bytesReceived  - 字节计数                  │
│ • retransmissions                   - 重传次数 (TPDC)           │
│ • timeouts                          - 超时次数                  │
│ • averageLatency / minLatency / maxLatency - 延迟统计           │
│ • throughputGbps                    - 吞吐量                    │
│ • errors / validationErrors         - 错误计数                  │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                     PdsStatistics                                │
├─────────────────────────────────────────────────────────────────┤
│ • totalPacketsSent / totalPacketsReceived                        │
│ • totalBytesSent / totalBytesReceived                            │
│ • pdcCreations / pdcDestructions                                 │
│ • errorCounts (by type)                                          │
│ • latencyMetrics (avg/min/max/jitter)                            │
└─────────────────────────────────────────────────────────────────┘
```

---

## 8. 快速参考

### 8.1 核心文件位置
```
src/soft-ue/
├── helper/
│   └── soft-ue-helper.h/cc           # 安装配置Helper
├── model/
│   ├── common/
│   │   ├── transport-layer.h         # 协议定义
│   │   └── soft-ue-packet-tag.h/cc   # 包标签
│   ├── ses/
│   │   ├── ses-manager.h/cc          # SES层管理器
│   │   ├── operation-metadata.h/cc   # 操作元数据
│   │   └── msn-entry.h/cc            # 消息序号表
│   ├── pds/
│   │   ├── pds-manager.h/cc          # PDS层管理器
│   │   ├── pds-common.h              # PDS通用定义
│   │   ├── pds-header.cc             # PDS头部
│   │   └── pds-statistics.cc         # 统计功能
│   ├── pdc/
│   │   ├── pdc-base.h/cc             # PDC基类
│   │   ├── ipdc.h/cc                 # 不可靠PDC
│   │   ├── tpdc.h/cc                 # 可靠PDC
│   │   └── rto-timer/                # RTO计时器
│   └── network/
│       ├── soft-ue-net-device.h/cc   # 网络设备
│       └── soft-ue-channel.h/cc      # 通道
└── test/
    └── *.cc                          # 测试文件
```

### 8.2 主程序入口
```
scratch/Soft-UE/Soft-UE.cc  # 完整端到端测试
```

---

*文档版本: 1.0.0*
*生成日期: 2025-12-16*
