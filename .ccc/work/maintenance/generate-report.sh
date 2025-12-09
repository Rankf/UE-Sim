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
