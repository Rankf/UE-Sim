#!/bin/bash

# Soft-UE Protocol Stack Validation Test
# Comprehensive validation script for core functionality
# Created: 2025-12-09

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
TEST_DIR="$PROJECT_ROOT/scratch"
RESULTS_DIR="$PROJECT_ROOT/validation-results"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Create results directory
mkdir -p "$RESULTS_DIR"

# Logging
log() { echo -e "${BLUE}[INFO]${NC} $1"; }
success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Test counter
TEST_COUNT=0
PASSED_COUNT=0

# Test wrapper
run_test() {
    local test_name="$1"
    local test_command="$2"
    local expected_result="${3:-0}"

    ((TEST_COUNT++))
    log "Running test $TEST_COUNT: $test_name"

    if eval "$test_command" > "$RESULTS_DIR/test_${TEST_COUNT}_${test_name// /_}.log" 2>&1; then
        success "✅ $test_name: PASSED"
        ((PASSED_COUNT++))
        return 0
    else
        error "❌ $test_name: FAILED"
        return 1
    fi
}

# Header
echo "===================================================================="
echo "              Soft-UE Protocol Stack Validation Test              "
echo "===================================================================="
echo "Project: $PROJECT_ROOT"
echo "Results: $RESULTS_DIR"
echo "Started: $(date)"
echo "===================================================================="

# Change to project directory
cd "$PROJECT_ROOT"

# Test 1: SES Manager Component Creation
run_test "SES Manager Component Creation" \
    "./ns3 run 'ses-manager-basic-test'" 0

# Test 2: PDS Manager Basic Functionality
run_test "PDS Manager Basic Functionality" \
    "./ns3 run 'pds-manager-basic-test'" 0

# Test 3: PDC Layer Operations
run_test "PDC Layer Operations" \
    "./ns3 run 'pdc-operations-test'" 0

# Test 4: Network Device Integration
run_test "Network Device Integration" \
    "./ns3 run 'soft-ue-net-device-test'" 0

# Test 5: Helper Functionality
run_test "Helper Functionality" \
    "./ns3 run 'soft-ue-helper-test'" 0

# Test 6: Basic Packet Flow
run_test "Basic Packet Flow" \
    "./ns3 run 'basic-packet-flow-test'" 0

# Test 7: SES-PDS Integration
run_test "SES-PDS Integration" \
    "./ns3 run 'ses-pds-integration-test'" 0

# Test 8: PDS-PDC Integration
run_test "PDS-PDC Integration" \
    "./ns3 run 'pds-pdc-integration-test'" 0

# Test 9: End-to-End Communication
run_test "End-to-End Communication" \
    "./ns3 run 'end-to-end-communication-test'" 0

# Test 10: Multi-Node Setup
run_test "Multi-Node Setup" \
    "./ns3 run 'multi-node-setup-test'" 0

# Test 11: Configuration Parameters
run_test "Configuration Parameters" \
    "./ns3 run 'configuration-test'" 0

# Test 12: Error Handling
run_test "Error Handling" \
    "./ns3 run 'error-handling-test'" 0

# Test 13: Performance Baseline
run_test "Performance Baseline" \
    "./ns3 run 'performance-baseline-test'" 0

# Test 14: Memory Usage
run_test "Memory Usage Validation" \
    "./ns3 run 'memory-usage-test'" 0

# Test 15: Build System Verification
run_test "Build System Verification" \
    "test -f build/lib/libns3.44-soft-ue.so && test -d build/src/soft-ue" 0

# Test 16: Documentation Completeness
run_test "Documentation Completeness" \
    "test -f docs/test-scripts-documentation.md && test -f PROJECT.md" 0

# Test 17: License Headers
run_test "License Headers Validation" \
    "find src/soft-ue -name '*.cc' -o -name '*.h' | head -5 | xargs grep -l 'Apache License'" 0

# Generate validation report
report_file="$RESULTS_DIR/validation-report-$(date +%Y%m%d_%H%M%S).txt"

cat > "$report_file" << EOF
Soft-UE Protocol Stack Validation Report
=======================================
Date: $(date)
Test Environment: $(uname -a)
Python Version: $(python3 --version 2>/dev/null || echo "Not found")

Test Summary:
------------
Total Tests: $TEST_COUNT
Passed: $PASSED_COUNT
Failed: $((TEST_COUNT - PASSED_COUNT))
Success Rate: $(( (PASSED_COUNT * 100) / TEST_COUNT ))%

Detailed Results:
----------------
EOF

# Add detailed test results to report
for i in $(seq 1 $TEST_COUNT); do
    for log_file in "$RESULTS_DIR"/test_${i}_*.log; do
        if [ -f "$log_file" ]; then
            test_name=$(basename "$log_file" .log | sed "s/test_${i}_//" | sed "s/_/ /g")
            status="PASSED"
            if grep -q "error\|Error\|ERROR\|failed\|Failed\|FAILED" "$log_file" 2>/dev/null; then
                status="FAILED"
            fi
            echo "Test $i: $test_name - $status" >> "$report_file"
        fi
    done
done

cat >> "$report_file" << EOF

Environment Check:
-----------------
- ns-3 Build: $([ -d "build" ] && echo "Available" || echo "Not found")
- Soft-UE Library: $([ -f "build/lib/libns3.44-soft-ue.so" ] && echo "Available" || echo "Not found")
- Test Scripts: $([ -d "scripts" ] && echo "Available" || echo "Not found")
- Documentation: $([ -f "docs/test-scripts-documentation.md" ] && echo "Available" || echo "Not found")

Recommendations:
---------------
EOF

if [ $PASSED_COUNT -eq $TEST_COUNT ]; then
    echo "✅ All validation tests passed! System is ready for production use." >> "$report_file"
elif [ $PASSED_COUNT -ge $((TEST_COUNT * 80 / 100)) ]; then
    echo "⚠️  Most tests passed. Review failed tests before production deployment." >> "$report_file"
else
    echo "❌ Significant test failures. Address issues before proceeding." >> "$report_file"
fi

echo ""
echo "Generated Files:"
echo "- Validation Report: $report_file"
echo "- Test Logs: $RESULTS_DIR/test_*.log"

# Final summary
echo "===================================================================="
echo "                    Validation Summary                           "
echo "===================================================================="
echo "Total Tests: $TEST_COUNT"
echo "Passed: $PASSED_COUNT"
echo "Failed: $((TEST_COUNT - PASSED_COUNT))"
echo "Success Rate: $(( (PASSED_COUNT * 100) / TEST_COUNT ))%"
echo ""
echo "Report: $report_file"
echo "===================================================================="

# Exit with appropriate code
if [ $PASSED_COUNT -eq $TEST_COUNT ]; then
    success "🎉 All validation tests passed!"
    exit 0
else
    error "💥 $((TEST_COUNT - PASSED_COUNT)) validation tests failed"
    exit 1
fi