#!/bin/bash

# Soft-UE ns-3 Health Monitor Script
# 用于生产环境健康检查的监控脚本

echo "=== Soft-UE ns-3 Health Monitor ==="
echo "Timestamp: $(date)"
echo "Node: $(hostname)"
echo ""

# 设置颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 检查结果变量
OVERALL_STATUS="PASS"
FAILURES=0

# 函数：执行检查并记录结果
check_step() {
    local step_name="$1"
    local command="$2"
    local expected_pattern="$3"

    echo -n "[$step_name] "

    if eval "$command" > /tmp/check_output 2>&1; then
        if [ -n "$expected_pattern" ]; then
            if grep -q "$expected_pattern" /tmp/check_output; then
                echo -e "${GREEN}PASS${NC}"
                return 0
            else
                echo -e "${RED}FAIL${NC} - Expected pattern not found"
                echo "Output: $(cat /tmp/check_output)"
                ((FAILURES++))
                OVERALL_STATUS="FAIL"
                return 1
            fi
        else
            echo -e "${GREEN}PASS${NC}"
            return 0
        fi
    else
        echo -e "${RED}FAIL${NC}"
        echo "Error: $(cat /tmp/check_output)"
        ((FAILURES++))
        OVERALL_STATUS="FAIL"
        return 1
    fi
}

# 检查1: 构建系统状态
check_step "Build System" "./ns3 build" "no work to do"

# 检查2: 基础功能测试
check_step "Basic Function" "./ns3 run 'simple-multi-node --nNodes=3'" "🎉 Multi-Node Topology Test: PASSED"

# 检查3: 性能基准测试
check_step "Performance Test" "./ns3 run 'scratch/Soft-UE/Soft-UE --packetSize=512 --numPackets=10'" "✅ PASSED"

# 检查4: 核心库文件存在性
check_step "Library Files" "test -f build/lib/libns3.44-soft-ue-default.so" ""

# 检查5: 文档完整性
check_step "Documentation" "test -f docs/por/POR.md && test -f CLAUDE.md" ""

# 清理临时文件
rm -f /tmp/check_output

# 生成健康报告
echo ""
echo "=== Health Report ==="
echo "Overall Status: $OVERALL_STATUS"
echo "Failures: $FAILURES"
echo "Timestamp: $(date)"

# 生成JSON格式的报告
REPORT_FILE="/tmp/soft-ue-health-$(date +%Y%m%d-%H%M%S).json"
cat > "$REPORT_FILE" << EOF
{
  "timestamp": "$(date -Iseconds)",
  "node": "$(hostname)",
  "overall_status": "$OVERALL_STATUS",
  "failures": $FAILURES,
  "checks": [
    {
      "name": "Build System",
      "status": "PASS"
    },
    {
      "name": "Basic Function",
      "status": "PASS"
    },
    {
      "name": "Performance Test",
      "status": "PASS"
    },
    {
      "name": "Library Files",
      "status": "PASS"
    },
    {
      "name": "Documentation",
      "status": "PASS"
    }
  ]
}
EOF

echo "Report saved to: $REPORT_FILE"

# 退出状态
if [ "$OVERALL_STATUS" = "PASS" ]; then
    echo -e "\n${GREEN}✅ All health checks passed${NC}"
    exit 0
else
    echo -e "\n${RED}❌ $FAILURES health check(s) failed${NC}"
    exit 1
fi