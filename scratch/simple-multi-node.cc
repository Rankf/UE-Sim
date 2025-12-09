#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/soft-ue-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimpleMultiNodeTest");

int main (int argc, char *argv[])
{
  // 配置参数
  uint32_t nNodes = 3;

  CommandLine cmd;
  cmd.AddValue ("nNodes", "Number of nodes", nNodes);
  cmd.Parse (argc, argv);

  std::cout << "=== Simple Multi-Node Soft-UE Test ===" << std::endl;
  std::cout << "Creating " << nNodes << " Soft-UE nodes..." << std::endl;

  // 创建节点
  NodeContainer nodes;
  nodes.Create (nNodes);

  // 创建Soft-UE网络
  SoftUeHelper softUe;

  // 安装Soft-UE设备
  NetDeviceContainer devices = softUe.Install (nodes);

  std::cout << "✓ Successfully installed Soft-UE devices on " << devices.GetN () << " nodes" << std::endl;

  // 验证每个设备的Soft-UE功能
  uint32_t validDevices = 0;
  for (uint32_t i = 0; i < devices.GetN (); ++i)
    {
      Ptr<SoftUeNetDevice> softUeDevice = DynamicCast<SoftUeNetDevice> (devices.Get (i));
      if (softUeDevice)
        {
          validDevices++;
          std::cout << "✓ Node " << i << ": SoftUeNetDevice valid" << std::endl;

          // 测试基本配置
          SoftUeConfig config = softUeDevice->GetConfiguration ();
          std::cout << "  - Max PDC Count: " << config.maxPdcCount << std::endl;
          std::cout << "  - Statistics Enabled: " << (config.enableStatistics ? "Yes" : "No") << std::endl;
        }
      else
        {
          std::cout << "✗ Node " << i << ": Invalid SoftUeNetDevice" << std::endl;
        }
    }

  // 测试节点间通信配置
  std::cout << "\n=== Multi-Node Communication Setup ===" << std::endl;

  for (uint32_t i = 0; i < nNodes; ++i)
    {
      Ptr<SoftUeNetDevice> device = DynamicCast<SoftUeNetDevice> (devices.Get (i));
      if (device)
        {
          // 设置每个节点的FEP (Forwarding Endpoint)
          uint32_t fepId = i + 1; // FEP IDs: 1, 2, 3

          SoftUeConfig config = device->GetConfiguration ();
          config.localFep = fepId;
          device->UpdateConfiguration (config);

          std::cout << "✓ Node " << i << " configured with FEP ID: " << fepId << std::endl;
        }
    }

  std::cout << "\n=== Test Results ===" << std::endl;
  std::cout << "Total Nodes: " << nNodes << std::endl;
  std::cout << "Valid Soft-UE Devices: " << validDevices << std::endl;
  std::cout << "Success Rate: " << (validDevices * 100 / nNodes) << "%" << std::endl;

  if (validDevices == nNodes)
    {
      std::cout << "\n🎉 Multi-Node Topology Test: PASSED" << std::endl;
      std::cout << "All nodes successfully configured with Soft-UE devices" << std::endl;
    }
  else
    {
      std::cout << "\n❌ Multi-Node Topology Test: FAILED" << std::endl;
      std::cout << "Some nodes failed to initialize properly" << std::endl;
    }

  std::cout << "\n=== Simple Multi-Node Test Complete ===" << std::endl;

  return 0;
}