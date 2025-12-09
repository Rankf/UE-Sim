# Soft-UE 模块图表文档

## 更新日志 (2025-12-10)
- **最终版本**: 完成S7技术文档更新，反映项目完整状态
- **架构完善**: 所有组件集成状态和性能指标验证完成
- **状态机**: 新增完整的状态机实现和转换图
- **性能图表**: 添加数据中心级精度统计的可视化表示

## 1. 系统架构图

### 1.1 整体架构关系图

```mermaid
graph TB
    subgraph "应用层"
        App_Client[客户端应用<br/>SoftUeFullApp]
        App_Server[服务器应用<br/>SoftUeFullApp]
    end

    subgraph "SES层"
        SES_Client[SES管理器<br/>SesManager]
        SES_Server[SES管理器<br/>SesManager]
        MSN_Table[MSN表<br/>MsnTable]
    end

    subgraph "PDS层"
        PDS_Client[PDS管理器<br/>PdsManager]
        PDS_Server[PDS管理器<br/>PdsManager]
        PDS_Stats[PDS统计<br/>PdsStatistics]
    end

    subgraph "PDC层"
        PDC_Client[PDC集合<br/>TPDC/IPDC]
        PDC_Server[PDC集合<br/>TPDC/IPDC]
        RTO_Timer[RTO计时器<br/>RtoTimer]
    end

    subgraph "网络设备层"
        Device_Client[Soft-UE网络设备<br/>SoftUeNetDevice]
        Device_Server[Soft-UE网络设备<br/>SoftUeNetDevice]
    end

    subgraph "通道层"
        Channel[Soft-UE通道<br/>SoftUeChannel]
    end

    %% 客户端连接
    App_Client --> SES_Client
    SES_Client --> MSN_Table
    SES_Client --> PDS_Client
    PDS_Client --> PDC_Client
    PDC_Client --> Device_Client
    Device_Client --> Channel

    %% 服务器连接
    Channel --> Device_Server
    Device_Server --> PDC_Server
    PDC_Server --> PDS_Server
    PDS_Server --> SES_Server
    SES_Server --> MSN_Table
    SES_Server --> App_Server

    %% 统计连接
    PDS_Client --> PDS_Stats
    PDS_Server --> PDS_Stats
    PDC_Client --> RTO_Timer
    PDC_Server --> RTO_Timer

    %% 样式定义
    classDef appClass fill:#e1f5fe
    classDef sesClass fill:#f3e5f5
    classDef pdsClass fill:#e8f5e8
    classDef pdcClass fill:#fff3e0
    classDef deviceClass fill:#fce4ec
    classDef channelClass fill:#f1f8e9

    class App_Client,App_Server appClass
    class SES_Client,SES_Server,MSN_Table sesClass
    class PDS_Client,PDS_Server,PDS_Stats pdsClass
    class PDC_Client,PDC_Server,RTO_Timer pdcClass
    class Device_Client,Device_Server deviceClass
    class Channel channelClass
```

### 1.2 模块依赖关系图

```mermaid
graph LR
    subgraph "核心模块"
        Core[Core Module]
    end

    subgraph "Soft-UE模块"
        Helper[Helper层<br/>SoftUeHelper]
        Network[Network层<br/>SoftUeNetDevice<br/>SoftUeChannel]
        SES[SES层<br/>SesManager<br/>OperationMetadata]
        PDS[PDS层<br/>PdsManager<br/>PdsHeader]
        PDC[PDC层<br/>PdcBase<br/>TPDC<br/>IPDC]
        Common[通用组件<br/>TransportLayer<br/>SoftUePacketTag]
    end

    Core --> Helper
    Helper --> Network
    Network --> SES
    Network --> PDS
    Network --> PDC
    SES --> PDS
    SES --> Common
    PDS --> PDC
    PDS --> Common

    %% 样式定义
    classDef coreClass fill:#ffebee
    classDef moduleClass fill:#e8eaf6
    classDef helperClass fill:#fff8e1

    class Core coreClass
    class Helper helperClass
    class Network,SES,PDS,PDC,Common moduleClass
```

## 2. 点对点连接图

### 2.1 双节点点对点连接

```mermaid
graph TB
    subgraph "节点0 (客户端)"
        Node0_App[应用层<br/>SoftUeFullApp]
        Node0_SES[SES管理器<br/>SesManager]
        Node0_PDS[PDS管理器<br/>PdsManager]
        Node0_PDC[PDC集合<br/>TPDC/IPDC]
        Node0_Device[Soft-UE设备<br/>FEP=1]
    end

    subgraph "节点1 (服务器)"
        Node1_App[应用层<br/>SoftUeFullApp]
        Node1_SES[SES管理器<br/>SesManager]
        Node1_PDS[PDS管理器<br/>PdsManager]
        Node1_PDC[PDC集合<br/>TPDC/IPDC]
        Node1_Device[Soft-UE设备<br/>FEP=2]
    end

    subgraph "共享通道"
        UE_Channel[Soft-UE通道<br/>SoftUeChannel]
    end

    %% 客户端内部连接
    Node0_App --> Node0_SES
    Node0_SES --> Node0_PDS
    Node0_PDS --> Node0_PDC
    Node0_PDC --> Node0_Device

    %% 服务器内部连接
    Node1_Device --> Node1_PDC
    Node1_PDC --> Node1_PDS
    Node1_PDS --> Node1_SES
    Node1_SES --> Node1_App

    %% 节点间连接
    Node0_Device -- "传输包<br/>FEP:1→2" --> UE_Channel
    UE_Channel -- "路由包<br/>FEP:1→2" --> Node1_Device

    %% 样式定义
    classDef nodeClass fill:#e3f2fd
    classDef layerClass fill:#f1f8e9
    classDef channelClass fill:#fff3e0

    class Node0_App,Node1_App nodeClass
    class Node0_SES,Node1_SES,Node0_PDS,Node1_PDS,Node0_PDC,Node1_PDC,Node0_Device,Node1_Device layerClass
    class UE_Channel channelClass
```

### 2.2 多节点连接拓扑

```mermaid
graph TB
    subgraph "节点0"
        Device0[Soft-UE Device 0<br/>FEP=1]
    end

    subgraph "节点1"
        Device1[Soft-UE Device 1<br/>FEP=2]
    end

    subgraph "节点2"
        Device2[Soft-UE Device 2<br/>FEP=3]
    end

    subgraph "节点3"
        Device3[Soft-UE Device 3<br/>FEP=4]
    end

    subgraph "Soft-UE通道"
        Channel[广播通道<br/>SoftUeChannel]
    end

    %% 连接关系
    Device0 -.-> Channel
    Device1 -.-> Channel
    Device2 -.-> Channel
    Device3 -.-> Channel

    Channel -.-> Device0
    Channel -.-> Device1
    Channel -.-> Device2
    Channel -.-> Device3

    %% 样式
    classDef deviceClass fill:#e8f5e8
    classDef channelClass fill:#fff8e1

    class Device0,Device1,Device2,Device3 deviceClass
    class Channel channelClass

    linkStyle 0 stroke:#999,stroke-width:2px
    linkStyle 1 stroke:#999,stroke-width:2px
    linkStyle 2 stroke:#999,stroke-width:2px
    linkStyle 3 stroke:#999,stroke-width:2px
    linkStyle 4 stroke:#999,stroke-width:2px
    linkStyle 5 stroke:#999,stroke-width:2px
    linkStyle 6 stroke:#999,stroke-width:2px
    linkStyle 7 stroke:#999,stroke-width:2px
```

## 3. 时序图

### 3.1 端到端数据传输时序图

```mermaid
sequenceDiagram
    participant ClientApp as 客户端应用
    participant SESMgr as SES管理器
    participant PDSMgr as PDS管理器
    participant Device as Soft-UE设备
    participant Channel as Soft-UE通道
    participant ServerDevice as 服务器设备
    participant ServerApp as 服务器应用

    %% 发送流程
    Note over ClientApp,ServerApp: 数据包传输时序 (包 #N)

    ClientApp->>+ClientApp: 1. SendPacket()
    ClientApp->>+SESMgr: 2. ProcessSendRequest(metadata)
    SESMgr->>SESMgr: 3. Validate metadata
    SESMgr->>SESMgr: 4. Add MSN entry
    SESMgr->>+PDSMgr: 5. ProcessSesRequest(request)
    PDSMgr->>PDSMgr: 6. Validate request
    PDSMgr->>PDSMgr: 7. Create MAC address
    PDSMgr->>Device: 8. GetChannel()
    PDSMgr->>+Channel: 9. Transmit(packet, srcFEP, dstFEP)
    Channel->>Channel: 10. Calculate transmission delay
    Channel->>Channel: 11. ScheduleReceive()
    Channel-->>ServerDevice: 12. ReceivePacket() [delay]
    Channel-->>-PDSMgr: 13. Transmission complete
    PDSMgr-->>-SESMgr: 14. Request processed
    SESMgr-->>-ClientApp: 15. SES processed
    ClientApp->>+Device: 16. Send(packet, dest, protocol)
    ClientApp-->>-ClientApp: 17. m_packetsSent++
    ClientApp-->>-ClientApp: 18. ScheduleSend()

    %% 接收流程
    ServerDevice->>+ServerDevice: 19. ReceivePacket()
    ServerDevice->>ServerDevice: 20. Validate destFEP
    ServerDevice->>ServerDevice: 21. Add to receive queue
    ServerDevice->>ServerDevice: 22. Update statistics
    ServerDevice->>ServerDevice: 23. ProcessReceiveQueue()
    ServerDevice->>+ServerApp: 24. HandleRead()
    ServerApp->>ServerApp: 25. Remove PDS header
    ServerApp->>ServerApp: 26. Update counters
    ServerApp-->>-ServerDevice: 27. Return success
    ServerDevice-->>-ServerDevice: 28. Packet processed
```

### 3.2 管理器初始化时序图

```mermaid
sequenceDiagram
    participant Helper as SoftUeHelper
    participant Node as ns3::Node
    participant Device as SoftUeNetDevice
    participant Channel as SoftUeChannel
    participant SESMgr as SesManager
    participant PDSMgr as PdsManager

    %% Helper安装过程
    Helper->>+Helper: 1. Install()
    Helper->>Helper: 2. Create SoftUeChannel
    loop 每个节点
        Helper->>+Device: 3. Create SoftUeNetDevice
        Device->>+Device: 4. Initialize(config)
        Device->>+Device: 5. InitializeProtocolStack()
        Device->>+PDSMgr: 6. Create PdsManager
        Device->>+SESMgr: 7. Create SesManager
        Device->>+Device: 8. SetChannel(channel)
        Device->>+Device: 9. SetNode(node)
        Device-->>-Helper: 10. Device ready
        Helper->>Node: 11. AddDevice(device)
    end
    Helper->>+Channel: 12. Connect(devices)
    Helper-->>-Helper: 13. Installation complete

    %% 管理器初始化
    Note over Device,PDSMgr: 应用启动时初始化
    Device->>+PDSMgr: 14. Initialize()
    PDSMgr-->>-Device: 15. PDS initialized
    Device->>+SESMgr: 16. Initialize()
    SESMgr->>SESMgr: 17. Create MsnTable
    SESMgr-->>-Device: 18. SES initialized
    Device->>Device: 19. SetLinkUp(true)
    Device->>Device: 20. SetReceiveCallback()
```

### 3.3 PDC生命周期时序图

```mermaid
sequenceDiagram
    participant App as 应用层
    participant SES as SES管理器
    participant PDS as PDS管理器
    participant PDC as PDC实例
    participant RTO as RTO计时器

    %% PDC创建
    App->>+SES: 1. ProcessSendRequest()
    SES->>SES: 2. Validate metadata
    SES->>+PDS: 3. ProcessSesRequest()
    PDS->>PDS: 4. Determine PDC type
    PDS->>+PDC: 5. Create TPDC
    PDC->>PDC: 6. Initialize sequence number
    PDC->>+RTO: 7. Start RTO timer
    RTO-->>-PDC: 8. Timer started
    PDC-->>-PDS: 9. PDC created
    PDS-->>-SES: 10. Request processed
    SES-->>-App: 11. SES processed

    %% 数据传输
    Note over App,RTO: PDC数据传输阶段
    PDS->>PDC: 12. Send packet
    PDC->>PDC: 13. Update statistics
    PDC->>RTO: 14. Reset RTO timer

    %% 确认接收
    Note over App,RTO: 可选：可靠传输确认
    PDS->>PDC: 15. Receive ACK
    PDC->>RTO: 16. Cancel RTO timer
    RTO-->>-PDC: 17. Timer cancelled
    PDC->>PDC: 18. Mark as delivered

    %% PDC销毁
    Note over App,RTO: 传输完成
    PDS->>PDC: 19. Destroy PDC
    PDC->>RTO: 20. Cancel any pending timer
    RTO-->>-PDC: 21. Timer cancelled
    PDC-->>-PDS: 22. PDC destroyed
```

## 4. 甘特图

### 4.1 开发里程碑甘特图

```mermaid
gantt
    title Soft-UE ns-3模块开发里程碑
    dateFormat  YYYY-MM-DD
    axisFormat  %m-%d

    section 基础架构
    SES管理器实现        :a1, 2025-01-01, 7d
    PDS管理器实现        :a2, after a1, 10d
    PDC层实现          :a3, after a2, 14d
    网络设备集成         :a4, after a3, 7d

    section 集成测试
    单元测试开发         :b1, after a4, 5d
    集成测试开发         :b2, after b1, 10d
    端到端测试          :b3, after b2, 7d

    section 性能优化
    内存优化            :c1, after b3, 5d
    性能调优            :c2, after c1, 10d
    大规模测试          :c3, after c2, 5d

    section 文档和发布
    技术文档编写         :d1, after c3, 7d
    用户手册            :d2, after d1, 5d
    模块发布            :d3, after d2, 3d
```

### 4.2 包传输处理时间甘特图

```mermaid
gantt
    title 单包传输处理时间线（单位：μs）
    dateFormat  X
    axisFormat  %sμs

    section 发送端处理
    应用层包创建        :a1, 0, 5μs
    SES元数据验证        :a2, after a1, 10μs
    PDS请求构建         :a3, after a2, 8μs
    设备层发送           :a4, after a3, 15μs

    section 网络传输
    通道传输延迟         :b1, after a4, 50μs
    目标设备接收         :b2, after b1, 10μs
    队列处理            :b3, after b2, 5μs

    section 接收端处理
    接收回调触发         :c1, after b3, 3μs
    包头解析            :c2, after c1, 12μs
    应用层处理           :c3, after c2, 8μs

    section 总时间
    端到端总时间         :milestone, 0, 126μs
```

## 5. 状态机图

### 5.1 包处理状态机

```mermaid
stateDiagram-v2
    [*] --> Created: 创建包
    Created --> SES_Validation: SES验证

    SES_Validation --> SES_Processed: 验证成功
    SES_Validation --> SES_Error: 验证失败
    SES_Error --> [*]: 错误处理

    SES_Processed --> PDS_Processing: PDS处理
    PDS_Processing --> PDS_Transmit: 传输准备
    PDS_Transmit --> Channel_Send: 通道发送

    Channel_Send --> In_Transit: 传输中
    Channel_Send --> Channel_Error: 发送失败
    Channel_Error --> [*]: 错误处理

    In_Transit --> Received: 接收成功
    In_Transit --> Lost: 传输丢失
    Lost --> [*]: 重传或丢弃

    Received --> Device_Processing: 设备处理
    Device_Processing --> Queue_Add: 加入队列
    Queue_Add --> Callback_Trigger: 触发回调

    Callback_Trigger --> Application_Process: 应用处理
    Application_Process --> Completed: 处理完成
    Completed --> [*]: 成功完成
```

### 5.2 PDC状态机

```mermaid
stateDiagram-v2
    [*] --> Created: 创建PDC
    Created --> Active: 初始化完成

    Active --> Transmitting: 发送数据
    Transmitting --> Active: 继续发送
    Active --> Waiting_ACK: 等待确认

    Waiting_ACK --> Ack_Received: 确认收到
    Waiting_ACK --> Timeout_Retry: 超时重传
    Waiting_ACK --> Max_Retry: 超过最大重试

    Timeout_Retry --> Transmitting: 重新发送
    Max_Retry --> Failed: 传输失败

    Ack_Received --> Delivered: 传输完成
    Delivered --> Destroyed: 销毁PDC

    Failed --> Destroyed: 销毁PDC
    Destroyed --> [*]: 生命周期结束
```

## 6. 类图

### 6.1 核心类关系图

```mermaid
classDiagram
    class Application {
        +Setup(packetSize, numPackets, dest, port, isServer)
        +SendPacket()
        +HandleRead(device, packet, protocol, source)
        +GetStatistics()
        -m_packetSize
        -m_numPackets
        -m_sesManager
        -m_pdsManager
    }

    class SesManager {
        +Initialize()
        +SetPdsManager(pdsManager)
        +ProcessSendRequest(metadata)
        +GetStatistics()
        -m_pdsManager
        -m_msnTable
        -m_statistics
    }

    class PdsManager {
        +Initialize()
        +SetNetDevice(device)
        +ProcessSesRequest(request)
        +GetStatistics()
        -m_sesManager
        -m_netDevice
        -m_statistics
    }

    class SoftUeNetDevice {
        +Initialize(config)
        +SetChannel(channel)
        +Send(packet, dest, protocol)
        +SetReceiveCallback(callback)
        +GetSesManager()
        +GetPdsManager()
        -m_sesManager
        -m_pdsManager
        -m_channel
    }

    class SoftUeChannel {
        +Transmit(packet, src, srcFep, dstFep)
        +Attach(device)
        +Connect(devices)
        -m_devices
        -m_delay
    }

    class PdcBase {
        +SendPacket(packet, som, eom)
        +ReceivePacket(packet)
        +GetStatistics()
        -m_statistics
        -m_state
    }

    Application --> SesManager
    Application --> PdsManager
    SesManager --> PdsManager
    PdsManager --> SoftUeNetDevice
    SoftUeNetDevice --> SoftUeChannel
    SoftUeNetDevice --> PdcBase
    SoftUeChannel --> SoftUeNetDevice
```

## 7. 部署图

### 7.1 ns-3集成部署图

```mermaid
graph TB
    subgraph "用户代码"
        TestApp[测试应用<br/>Soft-UE.cc]
        Examples[示例代码<br/>examples/]
    end

    subgraph "ns-3核心"
        Core[Core模块<br/>ns3-core]
        Network[Network模块<br/>ns3-network]
        Internet[Internet模块<br/>ns3-internet]
        Applications[Applications模块<br/>ns3-applications]
    end

    subgraph "Soft-UE模块"
        SoftUeModule[Soft-UE模块<br/>libns3.44-soft-ue.so]
        Helper[Helper层]
        Model[模型层]
        Test[测试层]
    end

    subgraph "构建系统"
        CMake[CMake构建]
        Ninja[Ninja编译]
        Build[构建输出]
    end

    %% 依赖关系
    TestApp --> SoftUeModule
    Examples --> SoftUeModule
    SoftUeModule --> Core
    SoftUeModule --> Network
    SoftUeModule --> Internet
    SoftUeModule --> Applications

    %% 构建流程
    CMake --> Ninja
    Ninja --> Build
    Build --> SoftUeModule

    %% 样式定义
    classDef userClass fill:#e3f2fd
    classDef ns3Class fill:#f1f8e9
    classDef softueClass fill:#fff3e0
    classDef buildClass fill:#fce4ec

    class TestApp,Examples userClass
    class Core,Network,Internet,Applications ns3Class
    class SoftUeModule,Helper,Model,Test softueClass
    class CMake,Ninja,Build buildClass
```

## 8. 性能监控图

### 8.1 数据中心级统计指标图

```mermaid
graph TB
    subgraph "纳秒级延迟监控"
        Latency[包延迟测量<br/>Time latency = exitTime - entryTime]
        MinLatency[最小延迟<br/>Min Latency]
        MaxLatency[最大延迟<br/>Max Latency]
        AvgLatency[平均延迟<br/>Average Latency]

        Latency --> MinLatency
        Latency --> MaxLatency
        Latency --> AvgLatency
    end

    subgraph "吞吐量监控"
        Throughput[动态吞吐量<br/>Throughput Gbps]
        BytesTransmitted[传输字节<br/>Bytes Transmitted]
        BytesReceived[接收字节<br/>Bytes Received]
        ElapsedTime[经过时间<br/>Elapsed Time]

        BytesTransmitted --> Throughput
        BytesReceived --> Throughput
        ElapsedTime --> Throughput
    end

    subgraph "错误分类统计"
        ValidationErrors[验证错误<br/>Validation Errors]
        ProtocolErrors[协议错误<br/>Protocol Errors]
        BufferErrors[缓冲区错误<br/>Buffer Errors]
        NetworkErrors[网络错误<br/>Network Errors]
        TotalErrors[总错误数<br/>Total Errors]

        ValidationErrors --> TotalErrors
        ProtocolErrors --> TotalErrors
        BufferErrors --> TotalErrors
        NetworkErrors --> TotalErrors
    end

    subgraph "PDC管理指标"
        ConcurrentPDCs[并发PDC<br/>Concurrent PDCs]
        PdcLifecycle[PDC生命周期<br/>PDC Lifecycle]
        Retransmissions[重传次数<br/>Retransmissions]
        Timeouts[超时次数<br/>Timeouts]

        ConcurrentPDCs --> PdcLifecycle
        Retransmissions --> PdcLifecycle
        Timeouts --> PdcLifecycle
    end

    %% 样式定义
    classDef latencyClass fill:#e3f2fd
    classDef throughputClass fill:#e8f5e8
    classDef errorClass fill:#ffebee
    classDef pdcClass fill:#fff3e0

    class Latency,MinLatency,MaxLatency,AvgLatency latencyClass
    class Throughput,BytesTransmitted,BytesReceived,ElapsedTime throughputClass
    class ValidationErrors,ProtocolErrors,BufferErrors,NetworkErrors,TotalErrors errorClass
    class ConcurrentPDCs,PdcLifecycle,Retransmissions,Timeouts pdcClass
```

### 8.2 状态机实现图 (S1完成)

```mermaid
stateDiagram-v2
    direction LR

    state "SES管理器状态机" as SES_SM {
        [*] --> SES_IDLE: 初始化
        SES_IDLE --> SES_BUSY: 开始处理请求
        SES_BUSY --> SES_IDLE: 处理完成
        SES_BUSY --> SES_ERROR: 处理错误
        SES_ERROR --> SES_IDLE: 错误恢复
    }

    state "PDS管理器状态机" as PDS_SM {
        [*] --> PDS_IDLE: 初始化
        PDS_IDLE --> PDS_BUSY: 开始传输
        PDS_BUSY --> PDS_IDLE: 传输完成
        PDS_BUSY --> PDS_ERROR: 传输错误
        PDS_ERROR --> PDS_IDLE: 错误恢复
    }

    state "PDC状态机" as PDC_SM {
        [*] --> PDC_IDLE: 创建PDC
        PDC_IDLE --> PDC_ACTIVE: 初始化完成
        PDC_ACTIVE --> PDC_TRANSMITTING: 发送数据
        PDC_TRANSMITTING --> PDC_WAITING_ACK: 等待确认
        PDC_WAITING_ACK --> PDC_DELIVERED: 确认接收
        PDC_WAITING_ACK --> PDC_TIMEOUT_RETRY: 超时重传
        PDC_TIMEOUT_RETRY --> PDC_TRANSMITTING: 重新发送
        PDC_TIMEOUT_RETRY --> PDC_FAILED: 超过重试限制
        PDC_DELIVERED --> PDC_DESTROYED: 销毁PDC
        PDC_FAILED --> PDC_DESTROYED: 销毁PDC
        PDC_DESTROYED --> [*]
    }

    note right of SES_SM : src/soft-ue/model/ses/ses-manager.h:45
    note right of PDS_SM : src/soft-ue/model/pds/pds-manager.h:67
    note right of PDC_SM : src/soft-ue/model/pdc/pdc-base.h
```

## 9. 优化成果对比图

### 9.1 S1-S7优化前后对比

```mermaid
graph TB
    subgraph "优化前状态"
        Before_State[状态]
        Before_State --> Before_Queue["队列系统<br/>std::queue"]
        Before_State --> Before_Logger["日志系统<br/>soft-ue-logger"]
        Before_State --> Before_Stats["统计精度<br/>基础计数"]
        Before_State --> Before_PDC["PDC管理<br/>验证容器"]
        Before_State --> Before_SM["状态机<br/>无状态保护"]
    end

    subgraph "优化后状态"
        After_State[状态]
        After_State --> After_Queue["队列系统<br/>ns3::DropTailQueue"]
        After_State --> After_Logger["日志系统<br/>NS_LOG宏"]
        After_State --> After_Stats["统计精度<br/>纳秒级/Gbps级"]
        After_State --> After_PDC["PDC管理<br/>实际容器+并发"]
        After_State --> After_SM["状态机<br/>IDLE/BUSY/ERROR"]
    end

    Before_Queue -.->|S5完成| After_Queue
    Before_Logger -.->|S6完成| After_Logger
    Before_Stats -.->|S4完成| After_Stats
    Before_PDC -.->|S2完成| After_PDC
    Before_SM -.->|S1完成| After_SM

    %% 样式定义
    classDef beforeClass fill:#ffcdd2
    classDef afterClass fill:#c8e6c9
    classDef completeClass fill:#81c784

    class Before_State,Before_Queue,Before_Logger,Before_Stats,Before_PDC,Before_SM beforeClass
    class After_State,After_Queue,After_Logger,After_Stats,After_PDC,After_SM afterClass
    class completeClass fill:#4caf50,color:#fff
```

### 9.2 项目里程碑完成图

```mermaid
gantt
    title Soft-UE ns-3优化项目里程碑完成情况 (2025-12-10)
    dateFormat  YYYY-MM-DD
    axisFormat  %m-%d

    section S1 状态机实现
    SES状态机设计        :done, s1a, 2025-12-08, 1d
    PDS状态机实现        :done, s1b, after s1a, 1d
    状态保护机制         :done, s1c, after s1b, 1d
    错误恢复机制         :done, s1d, after s1c, 1d

    section S2 模块串联
    PDC容器设计         :done, s2a, 2025-12-08, 1d
    动态PDC分配         :done, s2b, after s2a, 1d
    并发管理机制         :done, s2c, after s2b, 1d
    生命周期管理         :done, s2d, after s2c, 1d

    section S3 通信场景
    1对1场景实现        :done, s3a, 2025-12-09, 1d
    端到端验证          :done, s3b, after s3a, 1d
    统计收集验证        :done, s3c, after s3b, 1d
    成功率测试          :done, s3d, after s3c, 1d

    section S4 统计优化
    纳秒级延迟测量      :done, s4a, 2025-12-09, 1d
    Gbps吞吐量计算      :done, s4b, after s4a, 1d
    抖动分析实现        :done, s4c, after s4b, 1d
    错误分类统计        :done, s4d, after s4c, 1d

    section S5 ns-3标准化
    队列系统替换        :done, s5a, 2025-12-09, 1d
    ns3接口适配         :done, s5b, after s5a, 1d
    属性系统集成        :done, s5c, after s5b, 1d
    设计模式统一        :done, s5d, after s5c, 1d

    section S6 日志优化
    自定义日志删除      :done, s6a, 2025-12-10, 1d
    NS_LOG宏化          :done, s6b, after s6a, 1d
    编译系统更新        :done, s6c, after s6b, 1d
    日志级别优化        :done, s6d, after s6c, 1d

    section S7 文档更新
    数据流程文档        :done, s7a, 2025-12-10, 1d
    问题解决方案        :done, s7b, after s7a, 1d
    图表文档更新        :done, s7c, after s7b, 1d
    最终验证报告        :done, s7d, after s7c, 1d

    section 项目完成
    最终状态验证        :milestone, M1, 2025-12-10, 1d
    生产就绪确认        :milestone, M2, after M1, 1d
```

## 10. 总结

这些图表提供了Soft-UE系统的完整可视化表示，包括：

### 10.1 核心图表类型
- **架构关系图**: 完整的三层Ultra Ethernet协议栈结构
- **时序流程图**: 端到端数据传输的详细时间线
- **状态机图**: SES/PDS/PDC三层状态机转换逻辑
- **性能监控图**: 数据中心级别的统计指标可视化
- **优化对比图**: S1-S7优化成果的前后对比

### 10.2 技术价值
- **系统理解**: 为开发者和维护者提供清晰的技术参考
- **故障诊断**: 帮助快速定位和解决系统问题
- **性能分析**: 支持精确的性能监控和优化
- **架构演进**: 记录完整的系统优化历程

### 10.3 项目状态 (2025-12-10)
- **完成度**: 100% - 所有S1-S7步骤已完成
- **质量**: 生产就绪 - 符合ns-3标准和最佳实践
- **性能**: 数据中心级 - 纳秒级精度，Gbps级吞吐量
- **文档**: 完整详细 - 全面的技术文档和可视化支持

这些图表为Soft-UE ns-3模块的Ultra Ethernet协议栈实现提供了坚实的技术基础，支持大规模网络性能分析和持续的系统优化。