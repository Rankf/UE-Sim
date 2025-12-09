#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/soft-ue-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MultiNodeSoftUETest");

int main (int argc, char *argv[])
{
  // 配置参数
  uint32_t nNodes = 3;
  uint32_t packetSize = 512;
  uint32_t numPackets = 10;

  CommandLine cmd;
  cmd.AddValue ("nNodes", "Number of nodes", nNodes);
  cmd.AddValue ("packetSize", "Size of packets", packetSize);
  cmd.AddValue ("numPackets", "Number of packets", numPackets);
  cmd.Parse (argc, argv);

  NS_LOG_INFO ("=== Multi-Node Soft-UE Test ===");
  NS_LOG_INFO ("Nodes: " << nNodes << ", Packets: " << numPackets << ", Size: " << packetSize);

  // 创建节点
  NodeContainer nodes;
  nodes.Create (nNodes);

  // 创建Soft-UE网络
  SoftUeHelper softUe;

  // 配置Soft-UE设备属性
  softUe.SetDeviceAttribute ("MaxPdcCount", UintegerValue (1024));
  softUe.SetDeviceAttribute ("EnableStatistics", BooleanValue (true));

  // 安装Soft-UE设备
  NetDeviceContainer devices = softUe.Install (nodes);

  NS_LOG_INFO ("✓ Installed Soft-UE devices: " << devices.GetN ());

  // 安装网络协议栈
  InternetStackHelper internet;
  internet.Install (nodes);

  // 分配IP地址
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // 创建应用：Node 0 作为服务器，Node 1 和 Node 2 作为客户端
  uint16_t port = 9000;

  // 服务器应用 (Node 0)
  Address serverAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper serverHelper ("ns3::UdpSocketFactory", serverAddress);
  ApplicationContainer serverApps = serverHelper.Install (nodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  NS_LOG_INFO ("✓ Server installed on Node 0");

  // 客户端应用 (Node 1 和 Node 2)
  ApplicationContainer clientApps;

  for (uint32_t i = 1; i < nNodes; ++i)
    {
      OnOffHelper clientHelper ("ns3::UdpSocketFactory",
                               Address (InetSocketAddress (interfaces.GetAddress (0), port)));
      clientHelper.SetAttribute ("DataRate", StringValue ("5Mbps"));
      clientHelper.SetAttribute ("PacketSize", UintegerValue (packetSize));

      ApplicationContainer clientApp = clientHelper.Install (nodes.Get (i));
      clientApp.Start (Seconds (2.0 + i * 0.5)); // 错开启动时间
      clientApp.Stop (Seconds (8.0));
      clientApps.Add (clientApp);

      NS_LOG_INFO ("✓ Client " << i << " installed on Node " << i);
    }

  NS_LOG_INFO ("✓ All applications installed");

  // 简化测试，专注于核心功能验证
  NS_LOG_INFO ("✓ Multi-node topology configured");

  // 运行仿真
  NS_LOG_INFO ("Starting simulation...");
  Simulator::Run ();

  // 获取统计信息
  NS_LOG_INFO ("=== Simulation Results ===");

  for (uint32_t i = 0; i < nNodes; ++i)
    {
      Ptr<SoftUeNetDevice> softUeDevice = DynamicCast<SoftUeNetDevice> (devices.Get (i));
      if (softUeDevice)
        {
          SoftUeStats stats = softUeDevice->GetStatistics ();
          NS_LOG_INFO ("Node " << i << " Statistics:");
          NS_LOG_INFO ("  Packets Transmitted: " << stats.totalPacketsTransmitted);
          NS_LOG_INFO ("  Packets Received: " << stats.totalPacketsReceived);
          NS_LOG_INFO ("  Active PDCs: " << stats.activePdcCount);
        }
    }

  // 服务器接收统计
  Ptr<PacketSink> sink = serverApps.Get (0)->GetObject<PacketSink> ();
  if (sink)
    {
      NS_LOG_INFO ("Server Total Received: " << sink->GetTotalRx () << " bytes");
    }

  Simulator::Destroy ();

  NS_LOG_INFO ("=== Multi-Node Test Complete ===");

  return 0;
}