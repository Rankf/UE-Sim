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
 * @file             ses-manager-test.cc
 * @brief            SES Manager 单元测试
 * @author           softuegroup@gmail.com
 * @version          1.0.0
 * @date             2025-12-09
 * @copyright        Apache License Version 2.0
 *
 * @details
 * 测试SES管理器的核心功能：
 * - 消息类型处理
 * - 操作元数据管理
 * - MSN表操作
 * - 端点寻址
 */

#include "ns3/test.h"
#include "ns3/ses-manager.h"
#include "ns3/operation-metadata.h"
#include "ns3/msn-entry.h"

using namespace ns3;

/**
 * @brief SES Manager基础功能测试
 */
class SesManagerBasicTest : public TestCase
{
public:
    SesManagerBasicTest () : TestCase ("SES Manager Basic Test") {}

private:
    virtual void DoRun (void)
    {
        // 创建SES Manager实例
        Ptr<SesManager> sesManager = CreateObject<SesManager> ();
        NS_TEST_ASSERT_MSG_NE (sesManager, 0, "Unable to create SesManager");

        // 测试消息类型设置
        sesManager->SetMessageType ("DATA_TRANSFER", SesManager::DATA);
        NS_TEST_ASSERT_MSG_EQ (sesManager->GetMessageType ("DATA_TRANSFER"),
                               SesManager::DATA, "Message type not set correctly");

        // 测试操作元数据
        OperationMetadata metadata;
        metadata.SetOperationType ("SEND");
        metadata.SetSourceEndpoint (0);
        metadata.SetDestinationEndpoint (1);

        sesManager->RegisterOperation (metadata);
        OperationMetadata retrieved = sesManager->GetOperation ("SEND");
        NS_TEST_ASSERT_MSG_EQ (retrieved.GetSourceEndpoint (), 0,
                               "Source endpoint not preserved");
        NS_TEST_ASSERT_MSG_EQ (retrieved.GetDestinationEndpoint (), 1,
                               "Destination endpoint not preserved");
    }
};

/**
 * @brief SES Manager MSN表操作测试
 */
class SesManagerMsnTest : public TestCase
{
public:
    SesManagerMsnTest () : TestCase ("SES Manager MSN Test") {}

private:
    virtual void DoRun (void)
    {
        Ptr<SesManager> sesManager = CreateObject<SesManager> ();

        // 创建MSN条目
        MsnEntry entry;
        entry.SetMessageSequenceNumber (100);
        entry.SetEndpointId (5);
        entry.SetTimestamp (Simulator::Now ());

        // 添加MSN条目
        sesManager->AddMsnEntry (entry);

        // 查询MSN条目
        MsnEntry retrieved = sesManager->GetMsnEntry (100);
        NS_TEST_ASSERT_MSG_EQ (retrieved.GetEndpointId (), 5,
                               "MSN entry not found or incorrect");

        // 测试MSN计数
        uint32_t count = sesManager->GetMsnEntryCount ();
        NS_TEST_ASSERT_MSG_EQ (count, 1, "MSN entry count incorrect");
    }
};

/**
 * @brief SES Manager测试套件
 */
class SesManagerTestSuite : public TestSuite
{
public:
    SesManagerTestSuite () : TestSuite ("ses-manager", UNIT)
    {
        AddTestCase (new SesManagerBasicTest, TestCase::QUICK);
        AddTestCase (new SesManagerMsnTest, TestCase::QUICK);
    }
};

static SesManagerTestSuite sesManagerTestSuite;