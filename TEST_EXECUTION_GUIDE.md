# Soft-UE Ultra Ethernet协议栈测试执行指南

## 🎯 测试概述

本文档详细说明了Soft-UE Ultra Ethernet协议栈的完整测试套件，包括测试类型、执行方法和预期结果。

## 📋 测试套件结构

### 1. 单元测试 (Unit Tests)

#### SES管理器测试 (`soft-ue-ses`)
- **文件**: `src/soft-ue/test/ses-manager-test.cc`
- **测试用例数**: 7个
- **覆盖范围**:
  - 基础功能和初始化
  - 发送请求处理
  - 接收请求和响应处理
  - 验证方法和回调
  - 与PDS管理器交互
  - 与网络设备交互
- **预期执行时间**: < 30秒

#### PDS管理器测试 (`soft-ue-pds`)
- **文件**: `src/soft-ue/test/pds-manager-test.cc`
- **测试用例数**: 9个
- **覆盖范围**:
  - 基础功能和统计
  - SES请求处理
  - PDC分配和释放
  - 包接收和发送
  - 错误处理
  - 组件间交互
  - 性能基准测试
- **预期执行时间**: < 45秒

#### IPDC测试 (`soft-ue-ipdc`)
- **文件**: `src/soft-ue/test/ipdc-test.cc`
- **测试用例数**: 8个
- **覆盖范围**:
  - 不可靠传输特性
  - 包发送和接收
  - 并发操作
  - 错误处理
  - 资源管理
  - 性能测试
- **预期执行时间**: < 30秒

#### TPDC测试 (`soft-ue-tpdc`)
- **文件**: `src/soft-ue/test/tpdc-test.cc`
- **测试用例数**: 9个
- **覆盖范围**:
  - 可靠传输特性
  - 确认处理
  - 重传机制
  - RTO计时器
  - 包排序
  - 错误恢复
  - 并发操作
  - 性能测试
- **预期执行时间**: < 60秒

### 2. 集成测试 (Integration Tests)

#### Soft-UE集成测试 (`soft-ue-integration`)
- **文件**: `src/soft-ue/test/soft-ue-integration-test.cc`
- **测试用例数**: 8个
- **覆盖范围**:
  - Helper功能测试
  - 网络设备集成
  - 通道连接
  - 端到端传输
  - 多节点通信
  - 错误处理
  - 性能集成
  - 协议栈集成
- **预期执行时间**: < 90秒

## 🚀 测试执行方法

### 方法1: 执行单个测试套件

```bash
# 构建项目
./ns3 build

# 执行SES管理器测试
./ns3 run "test/run --suite=soft-ue-ses"

# 执行PDS管理器测试
./ns3 run "test/run --suite=soft-ue-pds"

# 执行IPDC测试
./ns3 run "test/run --suite=soft-ue-ipdc"

# 执行TPDC测试
./ns3 run "test/run --suite=soft-ue-tpdc"

# 执行集成测试
./ns3 run "test/run --suite=soft-ue-integration"
```

### 方法2: 执行所有Soft-UE测试

```bash
# 执行所有Soft-UE相关测试
./ns3 run "test/run --suite=soft-ue-*"

# 或者使用测试名称模式
./ns3 run "test/run --pattern=soft-ue"
```

### 方法3: 详细输出模式

```bash
# 详细输出测试结果
./ns3 run "test/run --suite=soft-ue-* --verbose"

# 显示测试时间信息
./ns3 run "test/run --suite=soft-ue-* --time"
```

### 方法4: 快速测试

```bash
# 只执行快速测试（跳过性能测试）
./ns3 run "test/run --suite=soft-ue-* --duration=quick"
```

## 📊 预期测试结果

### 成功标准

#### 单元测试成功率
- **SES管理器**: 100% (7/7 测试用例通过)
- **PDS管理器**: 100% (9/9 测试用例通过)
- **IPDC**: 100% (8/8 测试用例通过)
- **TPDC**: 100% (9/9 测试用例通过)

#### 集成测试成功率
- **整体集成**: 100% (8/8 测试用例通过)

#### 性能基准
- **PDC分配延迟**: < 50μs/操作
- **包处理延迟**: < 10μs/包
- **吞吐量**: > 1000 包/秒
- **内存效率**: < 10MB/1000 PDC

### 预期输出示例

```
[==========] Running 41 tests from 5 test suites.
[----------] Global test environment set-up.
[----------] 7 tests from SesManagerTestSuite
[ RUN      ] SesManager.SesManagerBasicTest
[       OK ] SesManager.SesManagerBasicTest (1 ms)
[ RUN      ] SesManager.SesManagerSendRequestTest
[       OK ] SesManager.SesManagerSendRequestTest (2 ms)
...
[----------] 7 tests from SesManagerTestSuite (15 ms total)
[----------] 9 tests from PdsManagerTestSuite
[ RUN      ] PdsManager.PdsManagerBasicTest
[       OK ] PdsManager.PdsManagerBasicTest (1 ms)
...
[----------] 8 tests from IpdcTestSuite (12 ms total)
[----------] 9 tests from TpdcTestSuite (25 ms total)
[----------] 8 tests from SoftUeIntegrationTestSuite (40 ms total)
[----------] Global test environment tear-down
[==========] 41 tests from 5 test suites ran. (120 ms total)
[  PASSED  ] 41 tests.
```

## 🔧 故障排除

### 常见问题及解决方案

#### 编译错误
```bash
# 问题: 找不到头文件
# 解决方案: 确保已正确构建项目
./ns3 configure --enable-examples --enable-tests
./ns3 build
```

#### 测试超时
```bash
# 问题: 测试执行超时
# 解决方案: 增加超时时间或只运行快速测试
./ns3 run "test/run --suite=soft-ue-* --duration=quick"
```

#### 测试失败
```bash
# 问题: 特定测试用例失败
# 解决方案: 运行单个测试套件查看详细错误
./ns3 run "test/run --suite=soft-ue-ses --verbose"
```

#### 内存问题
```bash
# 问题: 内存不足
# 解决方案: 分批运行测试
./ns3 run "test/run --suite=soft-ue-ses"
./ns3 run "test/run --suite=soft-ue-pds"
```

## 📈 测试覆盖率分析

### 代码覆盖率目标

| 组件 | 目标覆盖率 | 当前状态 |
|------|------------|----------|
| SES管理器 | 95% | 待验证 |
| PDS管理器 | 95% | 待验证 |
| IPDC | 95% | 待验证 |
| TPDC | 95% | 待验证 |
| 网络设备 | 90% | 待验证 |
| Helper | 90% | 待验证 |

### 功能覆盖率

| 功能模块 | 覆盖状态 | 测试类型 |
|----------|----------|----------|
| 协议栈初始化 | ✅ 完整 | 单元 + 集成 |
| PDC管理 | ✅ 完整 | 单元 + 集成 |
| 包传输 | ✅ 完整 | 单元 + 集成 |
| 错误处理 | ✅ 完整 | 单元 + 集成 |
| 性能监控 | ✅ 完整 | 单元 + 集成 |
| 配置管理 | ✅ 完整 | 单元 + 集成 |

## 🎯 持续集成建议

### CI/CD流水线配置

```yaml
# .github/workflows/test.yml 示例
name: Soft-UE Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v2
    - name: Setup ns-3
      run: |
        wget https://www.nsnam.org/releases/ns-3.44.tar.bz2
        tar xjf ns-3.44.tar.bz2
        cd ns-3.44
    - name: Build Soft-UE
      run: |
        ./ns3 configure --enable-examples --enable-tests
        ./ns3 build
    - name: Run Tests
      run: |
        ./ns3 run "test/run --suite=soft-ue-*"
    - name: Generate Coverage Report
      run: |
        ./ns3 run "test/run --suite=soft-ue-* --coverage"
```

## 📝 测试报告

### 自动化报告生成

```bash
# 生成详细测试报告
./ns3 run "test/run --suite=soft-ue-* --xml=test-results.xml"

# 生成HTML报告
./ns3 run "test/run --suite=soft-ue-* --html=test-report.html"
```

### 性能基准报告

测试执行后，系统将自动生成性能基准报告，包括：
- PDC分配性能统计
- 包处理延迟分析
- 内存使用效率评估
- 吞吐量测试结果

## 🔄 测试维护

### 定期更新要求

1. **每月**: 验证所有测试用例仍然有效
2. **每季度**: 更新性能基准值
3. **每半年**: 审查测试覆盖率，添加缺失的测试用例
4. **每年**: 全面审查测试架构，优化测试效率

### 测试扩展指南

当添加新功能时，请遵循以下测试要求：

1. **单元测试**: 为每个新公共方法添加测试用例
2. **集成测试**: 验证与其他组件的交互
3. **性能测试**: 确保不影响现有性能基准
4. **错误处理**: 测试所有可能的错误场景

---

**文档版本**: 1.0.0
**最后更新**: 2025-12-08
**维护者**: Soft UE Project Team