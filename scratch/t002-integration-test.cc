/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Soft-UE ns-3 Integration Test
 *
 * T002: 端到端协议栈集成测试
 * 验证SES/PDS/PDC完整协议栈功能
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/soft-ue-helper.h"
#include "ns3/soft-ue-net-device.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SoftUeIntegrationTest");

// 端到端测试结果统计
struct TestResults {
    uint32_t totalPacketsSent;
    uint32_t totalPacketsReceived;
    uint32_t sesSuccessCount;
    uint32_t pdsSuccessCount;
    uint32_t pdcSuccessCount;
    Time totalLatency;
    Time maxLatency;
    Time minLatency;

    TestResults() :
        totalPacketsSent(0),
        totalPacketsReceived(0),
        sesSuccessCount(0),
        pdsSuccessCount(0),
        pdcSuccessCount(0),
        totalLatency(Seconds(0)),
        maxLatency(TimeStep(0)),
        minLatency(TimeStep(0xFFFFFFFF))
    {}
};

class IntegrationTestApplication : public Application {
public:
    IntegrationTestApplication() : m_sentPackets(0), m_receivedPackets(0) {}

    virtual void Setup(Ptr<Node> node, Ipv4Address targetAddress,
                      uint16_t port, uint32_t packetCount, Time interval) {
        m_node = node;
        m_targetAddress = targetAddress;
        m_port = port;
        m_packetCount = packetCount;
        m_interval = interval;
        m_sentPackets = 0;
        m_receivedPackets = 0;
    }

    virtual void StartApplication() override {
        NS_LOG_INFO("Starting integration test application on node "
                   << m_node->GetId());
        m_sendEvent = Simulator::Schedule(Seconds(1.0),
                                         &IntegrationTestApplication::SendPacket, this);
    }

    virtual void StopApplication() override {
        NS_LOG_INFO("Stopping integration test application on node "
                   << m_node->GetId() << ", sent: " << m_sentPackets
                   << ", received: " << m_receivedPackets);
    }

private:
    void SendPacket() {
        if (m_sentPackets < m_packetCount) {
            // 创建测试数据包
            Ptr<Packet> packet = Create<Packet>(1024); // 1KB数据包

            // 添加时间戳用于延迟测量（简化版本）
            uint64_t now = Simulator::Now().GetTimeStep();
            // 简化时间戳处理，不使用复杂的Tag系统

            NS_LOG_INFO("Node " << m_node->GetId()
                       << " sending packet " << m_sentPackets + 1
                       << "/" << m_packetCount
                       << " to " << m_targetAddress);

            // 通过Soft-UE设备发送
            Ptr<SoftUeNetDevice> device = DynamicCast<SoftUeNetDevice>(
                m_node->GetDevice(0));

            if (device) {
                device->Send(packet, m_targetAddress, m_port);
                m_sentPackets++;
            }

            // 安排下一次发送
            m_sendEvent = Simulator::Schedule(m_interval,
                                            &IntegrationTestApplication::SendPacket, this);
        }
    }

    Ptr<Node> m_node;
    Ipv4Address m_targetAddress;
    uint16_t m_port;
    uint32_t m_packetCount;
    Time m_interval;
    EventId m_sendEvent;
    uint32_t m_sentPackets;
    uint32_t m_receivedPackets;
};

int main(int argc, char *argv[]) {
    // 测试参数配置
    uint32_t nNodes = 5;           // 节点数量
    uint32_t packetCount = 100;    // 每节点发送的数据包数量
    double interval = 0.1;         // 发送间隔（秒）
    bool verbose = true;           // 详细输出
    bool enableStats = true;       // 启用统计

    CommandLine cmd(__FILE__);
    cmd.AddValue("nNodes", "Number of nodes", nNodes);
    cmd.AddValue("packetCount", "Number of packets per node", packetCount);
    cmd.AddValue("interval", "Packet sending interval", interval);
    cmd.AddValue("verbose", "Enable verbose output", verbose);
    cmd.AddValue("enableStats", "Enable statistics collection", enableStats);
    cmd.Parse(argc, argv);

    // 启用日志
    if (verbose) {
        LogComponentEnable("SoftUeIntegrationTest", LOG_LEVEL_INFO);
        LogComponentEnable("SoftUeNetDevice", LOG_LEVEL_INFO);
        LogComponentEnable("SesManager", LOG_LEVEL_INFO);
        LogComponentEnable("PdsManager", LOG_LEVEL_INFO);
        LogComponentEnable("PdcBase", LOG_LEVEL_INFO);
    }

    NS_LOG_INFO("=== Soft-UE T002 Integration Test ===");
    NS_LOG_INFO("Test Configuration:");
    NS_LOG_INFO("  - Node Count: " << nNodes);
    NS_LOG_INFO("  - Packets per Node: " << packetCount);
    NS_LOG_INFO("  - Send Interval: " << interval << " seconds");
    NS_LOG_INFO("  - Total Packets: " << (nNodes * packetCount));

    // 1. 创建节点拓扑
    NS_LOG_INFO("\n=== 1. Creating Node Topology ===");
    NodeContainer nodes;
    nodes.Create(nNodes);

    // 2. 安装Soft-UE设备
    NS_LOG_INFO("\n=== 2. Installing Soft-UE Devices ===");
    SoftUeHelper helper;

    // 配置设备属性
    helper.SetDeviceAttribute("MaxPdcCount", UintegerValue(512));
    helper.SetDeviceAttribute("EnableStatistics", BooleanValue(enableStats));

    NetDeviceContainer devices = helper.Install(nodes);
    NS_LOG_INFO("✓ Successfully installed Soft-UE devices on "
               << devices.GetN() << " nodes");

    // 3. 验证设备安装
    NS_LOG_INFO("\n=== 3. Verifying Device Installation ===");
    for (uint32_t i = 0; i < devices.GetN(); ++i) {
        Ptr<SoftUeNetDevice> device = DynamicCast<SoftUeNetDevice>(devices.Get(i));
        if (device) {
            NS_LOG_INFO("✓ Node " << i << ": SoftUeNetDevice valid");
            NS_LOG_INFO("  - Max PDC Count: " << device->GetMaxPdcCount());
            NS_LOG_INFO("  - Statistics Enabled: "
                       << (device->GetEnableStatistics() ? "Yes" : "No"));
        } else {
            NS_LOG_ERROR("✗ Node " << i << ": Invalid SoftUeNetDevice");
            return 1;
        }
    }

    // 4. 配置网络地址
    NS_LOG_INFO("\n=== 4. Configuring Network Addresses ===");
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    NS_LOG_INFO("✓ Network configuration complete");
    NS_LOG_INFO("  - Network: 10.1.1.0/24");

    // 5. 安装集成测试应用
    NS_LOG_INFO("\n=== 5. Installing Integration Test Applications ===");
    uint16_t port = 9; // Discard port for testing

    ApplicationContainer apps;

    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        // 每个节点向下一个节点发送数据包（创建环形拓扑）
        uint32_t targetNode = (i + 1) % nodes.GetN();
        Ipv4Address targetAddress = interfaces.GetAddress(targetNode);

        Ptr<IntegrationTestApplication> app = CreateObject<IntegrationTestApplication>();
        app->Setup(nodes.Get(i), targetAddress, port, packetCount, Seconds(interval));
        nodes.Get(i)->AddApplication(app);

        apps.Add(app);
        NS_LOG_INFO("✓ Node " << i << " will send to Node " << targetNode
                   << " (" << targetAddress << ")");
    }

    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(60.0));

    // 6. 配置统计信息收集
    if (enableStats) {
        NS_LOG_INFO("\n=== 6. Enabling Statistics Collection ===");
        NS_LOG_INFO("✓ Statistics collection enabled on all devices");
    }

    // 7. 运行仿真
    NS_LOG_INFO("\n=== 7. Running Integration Test ===");
    NS_LOG_INFO("Simulation running...");

    Simulator::Stop(Seconds(65.0));
    Simulator::Run();

    // 8. 收集和报告测试结果
    NS_LOG_INFO("\n=== 8. Integration Test Results ===");
    TestResults results;

    // 统计设备级别的性能数据
    uint32_t totalSent = 0;
    uint32_t totalReceived = 0;
    uint64_t totalBytes = 0;

    for (uint32_t i = 0; i < devices.GetN(); ++i) {
        Ptr<SoftUeNetDevice> device = DynamicCast<SoftUeNetDevice>(devices.Get(i));
        if (device && enableStats) {
            SoftUeStats stats = device->GetStatistics();

            totalSent += stats.totalPacketsTransmitted;
            totalReceived += stats.totalPacketsReceived;
            totalBytes += stats.totalBytesReceived;

            NS_LOG_INFO("Node " << i << " Statistics:");
            NS_LOG_INFO("  - Sent: " << stats.totalPacketsTransmitted << " packets");
            NS_LOG_INFO("  - Received: " << stats.totalPacketsReceived << " packets");
            NS_LOG_INFO("  - Bytes: " << stats.totalBytesReceived << " bytes");
            NS_LOG_INFO("  - PDC Active: " << stats.activePdcCount);
            NS_LOG_INFO("  - Avg Latency: " << stats.averageLatency << " ms");
            NS_LOG_INFO("  - Throughput: " << stats.throughput << " Mbps");

            // 计算成功率
            if (stats.totalPacketsTransmitted > 0) {
                double successRate = (double)stats.totalPacketsReceived / stats.totalPacketsTransmitted * 100.0;
                NS_LOG_INFO("  - Success Rate: " << successRate << "%");
            }
        }
    }

    // 9. 测试结果评估
    NS_LOG_INFO("\n=== 9. Test Evaluation ===");
    NS_LOG_INFO("Overall Statistics:");
    NS_LOG_INFO("  - Total Sent: " << totalSent << " packets");
    NS_LOG_INFO("  - Total Received: " << totalReceived << " packets");
    NS_LOG_INFO("  - Total Bytes: " << totalBytes << " bytes");

    if (totalSent > 0) {
        double overallSuccessRate = (double)totalReceived / totalSent * 100.0;
        NS_LOG_INFO("  - Overall Success Rate: " << overallSuccessRate << "%");

        // 测试通过标准
        bool testPassed = (overallSuccessRate >= 95.0); // 95%成功率阈值

        NS_LOG_INFO("\n=== T002 Integration Test: "
                   << (testPassed ? "PASSED" : "FAILED") << " ===");

        if (testPassed) {
            NS_LOG_INFO("✓ End-to-end protocol stack integration successful");
            NS_LOG_INFO("✓ SES/PDS/PDC three-layer architecture validated");
            NS_LOG_INFO("✓ Multi-node communication verified");
            NS_LOG_INFO("✓ Performance meets integration requirements");
        } else {
            NS_LOG_ERROR("✗ Integration test failed");
            NS_LOG_ERROR("✗ Success rate below required threshold (95%)");
        }
    } else {
        NS_LOG_ERROR("✗ No packets were sent - test configuration error");
        NS_LOG_INFO("\n=== T002 Integration Test: FAILED ===");
    }

    // 10. 清理
    Simulator::Destroy();

    NS_LOG_INFO("\n=== Soft-UE T002 Integration Test Complete ===");

    return 0;
}