#!/bin/bash
# Manual monitoring trigger script

echo "Manual Soft-UE ns-3 Monitoring"
echo "=============================="

# 趋势监控
echo "Running trend monitoring..."
/home/makai/Soft-UE-ns3/.ccc/work/maintenance/20251210-014300/trend-monitor.sh

echo ""

# 健康检查
echo "Running health check..."
/home/makai/Soft-UE-ns3/.ccc/work/maintenance/20251210-014300/health-monitor.sh

echo ""
echo "Manual monitoring completed"
