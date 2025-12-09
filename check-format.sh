#!/bin/bash

# 代码格式化检查脚本
# 用于验证代码格式是否符合项目规范

echo "Soft-UE ns-3 代码格式化检查"
echo "=============================="

# 检查clang-format是否可用
if ! command -v clang-format &> /dev/null; then
    echo "⚠️  警告: clang-format 未安装，无法进行自动格式化检查"
    echo "   建议安装: sudo apt-get install clang-format"
    exit 1
fi

echo "✅ clang-format 工具可用"

# 检查.clang-format配置文件
if [ ! -f ".clang-format" ]; then
    echo "❌ 错误: 未找到 .clang-format 配置文件"
    exit 1
fi

echo "✅ 格式化配置文件存在"

# 检查主要源文件的格式
echo ""
echo "检查源文件格式..."

# 定义要检查的文件列表
FILES=(
    "src/soft-ue/model/ses/ses-manager.cc"
    "src/soft-ue/model/ses/ses-manager.h"
    "src/soft-ue/model/pds/pds-manager.cc"
    "src/soft-ue/model/pds/pds-manager.h"
    "src/soft-ue/model/network/soft-ue-net-device.cc"
    "src/soft-ue/model/pdc/pdc-base.cc"
)

format_errors=0

for file in "${FILES[@]}"; do
    if [ -f "$file" ]; then
        # 使用clang-format检查格式，--dry-run模式
        if ! clang-format --dry-run --Werror "$file" &> /dev/null; then
            echo "❌ $file 格式不正确"
            ((format_errors++))
        else
            echo "✅ $file 格式正确"
        fi
    else
        echo "⚠️  $file 文件不存在"
    fi
done

echo ""
echo "检查结果汇总:"
echo "=============="
if [ $format_errors -eq 0 ]; then
    echo "🎉 所有检查的文件格式都正确！"
    echo ""
    echo "建议定期运行以下命令维护代码格式:"
    echo "  find src/soft-ue -name '*.cc' -o -name '*.h' | xargs clang-format -i"
    exit 0
else
    echo "❌ 发现 $format_errors 个文件存在格式问题"
    echo ""
    echo "修复建议:"
    echo "  1. 安装 clang-format: sudo apt-get install clang-format"
    echo "  2. 运行自动修复: find src/soft-ue -name '*.cc' -o -name '*.h' | xargs clang-format -i"
    echo "  3. 或修复特定文件: clang-format -i <filename>"
    echo "  4. 参考 CODING_STYLE.md 了解详细规范"
    exit 1
fi