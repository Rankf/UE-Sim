/*******************************************************************************
 * Copyright 2025 Soft UE Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/

/**
 * @file             soft-ue-integration-test.cc
 * @brief            Soft-UE 集成测试
 * @author           softuegroup@gmail.com
 * @version          1.0.0
 * @date             2025-12-09
 * @copyright        Apache License Version 2.0
 *
 * @details
 * 测试Soft-UE模块的端到端集成：
 * - 网络设备功能
 * - SES/PDS Manager协作
 * - 多节点通信
 * - 数据包传输完整性
 */

#include "ns3/test.h"
#include "ns3/soft-ue-helper.h"
#include "ns3/soft-ue-net-device.h"
#include "ns3/soft-ue-channel.h"
#include "ns3/ses-manager.h"
#include "ns3/pds-manager.h"
#include "ns3/packet.h"
#include "ns3/node-container.h"
#include "ns3/net-device-container.h"

using namespace ns3;

/**
 * @brief Soft-UE基础集成测试
 */
class SoftUeBasicIntegrationTest : public TestCase
{
public:
    SoftUeBasicIntegrationTest () : TestCase ("Soft-UE Basic Integration Test") {}

private:
    virtual void DoRun (void)
    {
        // 创建节点
        NodeContainer nodes;
        nodes.Create (2);

        // 安装Soft-UE设备
        SoftUeHelper helper;
        NetDeviceContainer devices = helper.Install (nodes);

        NS_TEST_ASSERT_MSG_EQ (devices.GetN (), 2, "Not all devices installed");

        // 验证设备类型
        for (uint32_t i = 0; i < devices.GetN (); ++i)
        {
            Ptr<SoftUeNetDevice> device = devices.Get (i)->GetObject<SoftUeNetDevice> ();
            NS_TEST_ASSERT_MSG_NE (device, 0, "Device is not a SoftUeNetDevice");
        }

        // 测试设备属性
        Ptr<SoftUeNetDevice> device0 = devices.Get (0)->GetObject<SoftUeNetDevice> ();
        device0->SetAttribute ("MaxPdcCount", UintegerValue (512));

        UintegerValue maxPdcCount;
        device0->GetAttribute ("MaxPdcCount", maxPdcCount);
        NS_TEST_ASSERT_MSG_EQ (maxPdcCount.Get (), 512, "Device attribute not set");
    }
};

/**
 * @brief SES/PDS Manager协作测试
 */
class SoftUeManagerCooperationTest : public TestCase
{
public:
    SoftUeManagerCooperationTest () : TestCase ("Soft-UE Manager Cooperation Test") {}

private:
    virtual void DoRun (void)
    {
        // 创建Soft-UE网络设备
        Ptr<SoftUeNetDevice> device = CreateObject<SoftUeNetDevice> ();

        // 获取管理器实例
        Ptr<SesManager> sesManager = device->GetSesManager ();
        Ptr<PdsManager> pdsManager = device->GetPdsManager ();

        NS_TEST_ASSERT_MSG_NE (sesManager, 0, "SES Manager not available");
        NS_TEST_ASSERT_MSG_NE (pdsManager, 0, "PDS Manager not available");

        // 测试管理器协作
        // SES配置消息类型
        sesManager->SetMessageType ("DATA_TRANSFER", SesManager::DATA);

        // PDS配置路由
        pdsManager->SetRoutingAlgorithm ("SHORTEST_PATH");

        // 创建测试数据包
        Ptr<Packet> packet = Create<Packet> (1024);

        // 添加SES标签
        SoftUePacketTag tag;
        tag.SetMessageType ("DATA_TRANSFER");
        tag.SetSourceEndpoint (0);
        tag.SetDestinationEndpoint (1);
        packet->AddPacketTag (tag);

        // 通过PDS处理数据包
        bool processed = pdsManager->ProcessPacket (packet);
        NS_TEST_ASSERT_MSG_EQ (processed, true, "PDS processing failed");
    }
};

/**
 * @brief 多节点通信测试
 */
class SoftUeMultiNodeTest : public TestCase
{
public:
    SoftUeMultiNodeTest () : TestCase ("Soft-UE Multi-Node Communication Test") {}

private:
    virtual void DoRun (void)
    {
        // 创建多个节点
        NodeContainer nodes;
        nodes.Create (5);

        // 安装Soft-UE设备
        SoftUeHelper helper;
        NetDeviceContainer devices = helper.Install (nodes);

        NS_TEST_ASSERT_MSG_EQ (devices.GetN (), 5, "Not all devices installed");

        // 测试节点间通信
        uint32_t successfulTransmissions = 0;
        const uint32_t totalTests = 20; // 5 nodes * 4 destinations

        for (uint32_t src = 0; src < nodes.GetN (); ++src)
        {
            for (uint32_t dst = 0; dst < nodes.GetN (); ++dst)
            {
                if (src == dst) continue; // 跳过自己

                Ptr<SoftUeNetDevice> srcDevice = devices.Get (src)->GetObject<SoftUeNetDevice> ();
                Ptr<Packet> packet = Create<Packet> (512);

                // 设置目标节点
                SoftUePacketTag tag;
                tag.SetDestinationEndpoint (dst);
                packet->AddPacketTag (tag);

                // 发送数据包
                bool sent = srcDevice->Send (packet, devices.Get (dst)->GetAddress (), 0x0800);
                if (sent)
                {
                    successfulTransmissions++;
                }
            }
        }

        // 验证传输成功率
        double successRate = (double)successfulTransmissions / totalTests * 100.0;
        NS_TEST_ASSERT_MSG_GT (successRate, 80.0, "Multi-node communication success rate too low");
    }
};

/**
 * @brief 端到端数据完整性测试
 */
class SoftUeEndToEndTest : public TestCase
{
public:
    SoftUeEndToEndTest () : TestCase ("Soft-UE End-to-End Data Integrity Test") {}

private:
    virtual void DoRun (void)
    {
        // 创建源和目标节点
        NodeContainer nodes;
        nodes.Create (2);

        SoftUeHelper helper;
        NetDeviceContainer devices = helper.Install (nodes);

        Ptr<SoftUeNetDevice> srcDevice = devices.Get (0)->GetObject<SoftUeNetDevice> ();
        Ptr<SoftUeNetDevice> dstDevice = devices.Get (1)->GetObject<SoftUeNetDevice> ();

        // 创建测试数据
        const std::string testData = "Soft-UE Integration Test Data";
        Ptr<Packet> originalPacket = Create<Packet> ((uint8_t*)testData.c_str (), testData.length ());

        // 添加测试标签
        SoftUePacketTag tag;
        tag.SetMessageType ("INTEGRATION_TEST");
        tag.SetSourceEndpoint (0);
        tag.SetDestinationEndpoint (1);
        originalPacket->AddPacketTag (tag);

        // 发送数据包
        bool sent = srcDevice->Send (originalPacket, dstDevice->GetAddress (), 0x0800);
        NS_TEST_ASSERT_MSG_EQ (sent, true, "Packet sending failed");

        // 验证设备统计
        uint64_t sentPackets = srcDevice->GetSentPacketCount ();
        uint64_t receivedPackets = dstDevice->GetReceivedPacketCount ();

        NS_TEST_ASSERT_MSG_EQ (sentPackets, 1, "Sent packet count incorrect");
        NS_TEST_ASSERT_MSG_EQ (receivedPackets, 1, "Received packet count incorrect");
    }
};

/**
 * @brief Soft-UE集成测试套件
 */
class SoftUeIntegrationTestSuite : public TestSuite
{
public:
    SoftUeIntegrationTestSuite () : TestSuite ("soft-ue-integration", SYSTEM)
    {
        AddTestCase (new SoftUeBasicIntegrationTest, TestCase::QUICK);
        AddTestCase (new SoftUeManagerCooperationTest, TestCase::QUICK);
        AddTestCase (new SoftUeMultiNodeTest, TestCase::QUICK);
        AddTestCase (new SoftUeEndToEndTest, TestCase::QUICK);
    }
};

static SoftUeIntegrationTestSuite softUeIntegrationTestSuite;