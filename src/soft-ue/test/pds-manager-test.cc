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
 * @file             pds-manager-test.cc
 * @brief            PDS Manager 单元测试
 * @author           softuegroup@gmail.com
 * @version          1.0.0
 * @date             2025-12-09
 * @copyright        Apache License Version 2.0
 *
 * @details
 * 测试PDS管理器的核心功能：
 * - 包分发和路由
 * - PDC管理和协调
 * - 统计信息收集
 * - 性能基准测试
 */

#include "ns3/test.h"
#include "ns3/pds-manager.h"
#include "ns3/pds-common.h"
#include "ns3/pdc-base.h"
#include "ns3/packet.h"

using namespace ns3;

/**
 * @brief PDS Manager基础功能测试
 */
class PdsManagerBasicTest : public TestCase
{
public:
    PdsManagerBasicTest () : TestCase ("PDS Manager Basic Test") {}

private:
    virtual void DoRun (void)
    {
        // 创建PDS Manager实例
        Ptr<PdsManager> pdsManager = CreateObject<PdsManager> ();
        NS_TEST_ASSERT_MSG_NE (pdsManager, 0, "Unable to create PdsManager");

        // 测试路由算法设置
        pdsManager->SetRoutingAlgorithm ("SHORTEST_PATH");
        std::string algorithm = pdsManager->GetRoutingAlgorithm ();
        NS_TEST_ASSERT_MSG_EQ (algorithm, "SHORTEST_PATH",
                               "Routing algorithm not set correctly");

        // 测试统计功能
        pdsManager->EnableStatistics (true);
        bool statsEnabled = pdsManager->IsStatisticsEnabled ();
        NS_TEST_ASSERT_MSG_EQ (statsEnabled, true,
                               "Statistics not enabled correctly");

        // 创建测试数据包
        Ptr<Packet> packet = Create<Packet> (1024);
        NS_TEST_ASSERT_MSG_NE (packet, 0, "Unable to create test packet");

        // 测试包处理
        bool processed = pdsManager->ProcessPacket (packet);
        NS_TEST_ASSERT_MSG_EQ (processed, true,
                               "Packet processing failed");
    }
};

/**
 * @brief PDS Manager PDC管理测试
 */
class PdsManagerPdcTest : public TestCase
{
public:
    PdsManagerPdcTest () : TestCase ("PDS Manager PDC Test") {}

private:
    virtual void DoRun (void)
    {
        Ptr<PdsManager> pdsManager = CreateObject<PdsManager> ();

        // 创建PDC实例
        Ptr<PdcBase> pdc1 = CreateObject<PdcBase> ();
        Ptr<PdcBase> pdc2 = CreateObject<PdcBase> ();

        NS_TEST_ASSERT_MSG_NE (pdc1, 0, "Unable to create PDC 1");
        NS_TEST_ASSERT_MSG_NE (pdc2, 0, "Unable to create PDC 2");

        // 注册PDC
        uint32_t pdcId1 = pdsManager->RegisterPdc (pdc1);
        uint32_t pdcId2 = pdsManager->RegisterPdc (pdc2);

        NS_TEST_ASSERT_MSG_EQ (pdsId1, 0, "First PDC ID should be 0");
        NS_TEST_ASSERT_MSG_EQ (pdsId2, 1, "Second PDC ID should be 1");

        // 测试PDC查询
        Ptr<PdcBase> retrieved1 = pdsManager->GetPdc (pdcId1);
        Ptr<PdcBase> retrieved2 = pdsManager->GetPdc (pdcId2);

        NS_TEST_ASSERT_MSG_EQ (retrieved1, pdc1, "PDC 1 not retrieved correctly");
        NS_TEST_ASSERT_MSG_EQ (retrieved2, pdc2, "PDC 2 not retrieved correctly");

        // 测试PDC计数
        uint32_t count = pdsManager->GetPdcCount ();
        NS_TEST_ASSERT_MSG_EQ (count, 2, "PDC count incorrect");
    }
};

/**
 * @brief PDS Manager性能测试
 */
class PdsManagerPerformanceTest : public TestCase
{
public:
    PdsManagerPerformanceTest () : TestCase ("PDS Manager Performance Test") {}

private:
    virtual void DoRun (void)
    {
        Ptr<PdsManager> pdsManager = CreateObject<PdsManager> ();

        // 性能测试：处理大量数据包
        Time startTime = Simulator::Now ();
        const uint32_t packetCount = 1000;
        const uint32_t packetSize = 1024;

        for (uint32_t i = 0; i < packetCount; ++i)
        {
            Ptr<Packet> packet = Create<Packet> (packetSize);
            pdsManager->ProcessPacket (packet);
        }

        Time endTime = Simulator::Now ();
        Time processingTime = endTime - startTime;

        // 验证性能：应在合理时间内完成
        NS_TEST_ASSERT_MSG_LT (processingTime.GetMicroSeconds (), 100000,
                               "Packet processing too slow");

        // 验证统计信息
        uint64_t processedPackets = pdsManager->GetProcessedPacketCount ();
        NS_TEST_ASSERT_MSG_EQ (processedPackets, packetCount,
                               "Processed packet count incorrect");
    }
};

/**
 * @brief PDS Manager测试套件
 */
class PdsManagerTestSuite : public TestSuite
{
public:
    PdsManagerTestSuite () : TestSuite ("pds-manager", UNIT)
    {
        AddTestCase (new PdsManagerBasicTest, TestCase::QUICK);
        AddTestCase (new PdsManagerPdcTest, TestCase::QUICK);
        AddTestCase (new PdsManagerPerformanceTest, TestCase::QUICK);
    }
};

static PdsManagerTestSuite pdsManagerTestSuite;