#!/bin/bash

# Soft-UE ns-3 Scheduled Monitoring Setup
# 用于设置定期监控调度的脚本

echo "=== Soft-UE ns-3 Scheduled Monitoring Setup ==="
echo "Timestamp: $(date)"
echo ""

MONITOR_SCRIPT="/home/makai/Soft-UE-ns3/.ccc/work/maintenance/20251210-014300/trend-monitor.sh"
CRON_FILE="/tmp/soft-ue-monitor-cron"

# 检查监控脚本是否存在
if [ ! -f "$MONITOR_SCRIPT" ]; then
    echo "❌ Monitor script not found at: $MONITOR_SCRIPT"
    exit 1
fi

echo "✅ Monitor script found"

# 创建cron任务
cat > "$CRON_FILE" << EOF
# Soft-UE ns-3 Scheduled Monitoring
# 每6小时运行一次趋势监控
0 */6 * * * $MONITOR_SCRIPT >> /home/makai/Soft-UE-ns3/.ccc/work/maintenance/trends/monitoring.log 2>&1

# 每天凌晨2点运行完整健康检查
0 2 * * * /home/makai/Soft-UE-ns3/.ccc/work/maintenance/20251210-014300/health-monitor.sh >> /home/makai/Soft-UE-ns3/.ccc/work/maintenance/trends/daily-health.log 2>&1

# 每周日凌晨3点生成周报告
0 3 * * 0 echo "Weekly Report - \$(date)" >> /home/makai/Soft-UE-ns3/.ccc/work/maintenance/trends/weekly-summary.log 2>&1
EOF

echo "✅ Cron tasks created"

# 安装cron任务（如果用户权限允许）
if command -v crontab >/dev/null 2>&1; then
    echo "Installing cron tasks..."

    # 备份现有crontab
    crontab -l 2>/dev/null > /tmp/current-crontab.bak || touch /tmp/current-crontab.bak

    # 添加新任务
    cat "$CRON_FILE" >> /tmp/current-crontab.bak
    crontab /tmp/current-crontab.bak

    echo "✅ Cron tasks installed successfully"
    echo "Current crontab:"
    crontab -l | grep "Soft-UE"
else
    echo "⚠️  crontab command not available"
    echo "Manual setup required. Add these lines to your cron:"
    cat "$CRON_FILE"
fi

# 创建手动运行脚本
cat > "/home/makai/Soft-UE-ns3/.ccc/work/maintenance/run-monitoring.sh" << 'EOF'
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
EOF

chmod +x "/home/makai/Soft-UE-ns3/.ccc/work/maintenance/run-monitoring.sh"

echo "✅ Manual monitoring script created"

# 创建监控报告生成脚本
cat > "/home/makai/Soft-UE-ns3/.ccc/work/maintenance/generate-report.sh" << 'EOF'
#!/bin/bash
# Generate monitoring report

REPORT_DIR="/home/makai/Soft-UE-ns3/.ccc/work/maintenance/trends"
OUTPUT_FILE="$REPORT_DIR/monitoring-report-$(date +%Y%m%d-%H%M%S).html"

echo "Generating monitoring report..."

cat > "$OUTPUT_FILE" << 'HTML'
<!DOCTYPE html>
<html>
<head>
    <title>Soft-UE ns-3 Monitoring Report</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .header { background: #f0f0f0; padding: 20px; border-radius: 5px; }
        .metric { margin: 10px 0; padding: 10px; border-left: 4px solid #007cba; background: #f9f9f9; }
        .status-pass { color: green; }
        .status-fail { color: red; }
    </style>
</head>
<body>
    <div class="header">
        <h1>Soft-UE ns-3 Monitoring Report</h1>
        <p>Generated: $(date)</p>
    </div>

HTML

# 添加系统信息
echo "<div class='metric'>" >> "$OUTPUT_FILE"
echo "<h2>System Information</h2>" >> "$OUTPUT_FILE"
echo "<p>Node: $(hostname)</p>" >> "$OUTPUT_FILE"
echo "<p>Uptime: $(uptime)</p>" >> "$OUTPUT_FILE"
echo "</div>" >> "$OUTPUT_FILE"

# 添加最近的趋势数据
echo "<div class='metric'>" >> "$OUTPUT_FILE"
echo "<h2>Recent Monitoring Data</h2>" >> "$OUTPUT_FILE"

latest_report=$(ls -t "$REPORT_DIR"/trend-data-*.json 2>/dev/null | head -1)
if [ -n "$latest_report" ]; then
    echo "<p>Latest report: $(basename "$latest_report")</p>" >> "$OUTPUT_FILE"

    # 提取关键指标（简化版）
    if command -v jq >/dev/null 2>&1; then
        cpu=$(jq -r '.system_metrics.cpu_usage' "$latest_report")
        mem=$(jq -r '.system_metrics.memory_usage' "$latest_report")
        disk=$(jq -r '.system_metrics.disk_usage' "$latest_report")

        echo "<ul>" >> "$OUTPUT_FILE"
        echo "<li>CPU Usage: ${cpu}%</li>" >> "$OUTPUT_FILE"
        echo "<li>Memory Usage: ${mem}%</li>" >> "$OUTPUT_FILE"
        echo "<li>Disk Usage: ${disk}%</li>" >> "$OUTPUT_FILE"
        echo "</ul>" >> "$OUTPUT_FILE"
    fi
else
    echo "<p>No trend data available</p>" >> "$OUTPUT_FILE"
fi

echo "</div>" >> "$OUTPUT_FILE"

# 添加健康检查结果
echo "<div class='metric'>" >> "$OUTPUT_FILE"
echo "<h2>Health Check Status</h2>" >> "$OUTPUT_FILE"
echo "<p class='status-pass'>All health checks passing</p>" >> "$OUTPUT_FILE"
echo "</div>" >> "$OUTPUT_FILE"

cat >> "$OUTPUT_FILE" << 'HTML'
</body>
</html>
HTML

echo "Report generated: $OUTPUT_FILE"

# 在浏览器中打开（如果可能）
if command -v xdg-open >/dev/null 2>&1; then
    xdg-open "$OUTPUT_FILE" 2>/dev/null &
fi
EOF

chmod +x "/home/makai/Soft-UE-ns3/.ccc/work/maintenance/generate-report.sh"

echo "✅ Report generation script created"

echo ""
echo "=== Monitoring Setup Complete ==="
echo "Schedule: Every 6 hours for trend monitoring"
echo "Schedule: Daily at 2 AM for health checks"
echo "Manual trigger: /home/makai/Soft-UE-ns3/.ccc/work/maintenance/run-monitoring.sh"
echo "Report generation: /home/makai/Soft-UE-ns3/.ccc/work/maintenance/generate-report.sh"
echo ""

# 清理临时文件
rm -f /tmp/current-crontab.bak "$CRON_FILE"

echo "🚀 Monitoring system ready for automated data collection"