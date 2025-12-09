#!/bin/bash

# Soft-UE Protocol Stack Test Runner
# Created: 2025-12-09
# Version: 1.0.0
# Description: Comprehensive test script for Soft-UE ns-3 implementation

set -e  # Exit on any error

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
TEST_RESULTS_DIR="$PROJECT_ROOT/test-results"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

# Test configuration
ENABLE_COVERAGE=true
ENABLE_PERFORMANCE_TEST=true
ENABLE_STRESS_TEST=true
VERBOSE_OUTPUT=true

# Initialize directories
mkdir -p "$TEST_RESULTS_DIR"
mkdir -p "$TEST_RESULTS_DIR/coverage"
mkdir -p "$TEST_RESULTS_DIR/performance"
mkdir -p "$TEST_RESULTS_DIR/logs"

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Print test header
print_header() {
    echo "======================================================================"
    echo "                   Soft-UE Protocol Stack Test Suite                "
    echo "======================================================================"
    echo "Project Root: $PROJECT_ROOT"
    echo "Build Directory: $BUILD_DIR"
    echo "Test Results: $TEST_RESULTS_DIR"
    echo "Timestamp: $TIMESTAMP"
    echo "======================================================================"
}

# Check if build exists and is up to date
check_build() {
    log_info "Checking build status..."

    if [ ! -f "$BUILD_DIR/lib/libns3.44-soft-ue.so" ]; then
        log_warning "Soft-UE library not found. Starting build process..."
        build_project
    else
        log_success "Soft-UE library found"
    fi
}

# Build the project
build_project() {
    log_info "Building Soft-UE project..."

    cd "$PROJECT_ROOT"

    if [ ! -d "$BUILD_DIR" ]; then
        mkdir -p "$BUILD_DIR"
        cd "$BUILD_DIR"
        cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..
    else
        cd "$BUILD_DIR"
    fi

    if command -v ninja &> /dev/null; then
        ninja
    else
        make -j$(nproc)
    fi

    if [ $? -eq 0 ]; then
        log_success "Build completed successfully"
    else
        log_error "Build failed"
        exit 1
    fi
}

# Run unit tests
run_unit_tests() {
    log_info "Running unit tests..."

    cd "$BUILD_DIR"

    # List of unit test executables
    declare -a UNIT_TESTS=(
        "test/ses-manager-test"
        "test/pds-manager-test"
        "test/pds-full-test"
    )

    local total_tests=${#UNIT_TESTS[@]}
    local passed_tests=0

    for test in "${UNIT_TESTS[@]}"; do
        if [ -f "$test" ]; then
            log_info "Running $(basename $test)..."

            if [ "$VERBOSE_OUTPUT" = true ]; then
                ./"$test" 2>&1 | tee "$TEST_RESULTS_DIR/logs/$(basename $test).log"
                local result=${PIPESTATUS[0]}
            else
                ./"$test" > "$TEST_RESULTS_DIR/logs/$(basename $test).log" 2>&1
                local result=$?
            fi

            if [ $result -eq 0 ]; then
                log_success "✅ $(basename $test) passed"
                ((passed_tests++))
            else
                log_error "❌ $(basename $test) failed (exit code: $result)"
            fi
        else
            log_warning "⚠️  Test binary not found: $test"
        fi
    done

    log_info "Unit tests summary: $passed_tests/$total_tests passed"

    if [ $passed_tests -eq $total_tests ]; then
        return 0
    else
        return 1
    fi
}

# Run integration tests
run_integration_tests() {
    log_info "Running integration tests..."

    cd "$PROJECT_ROOT"

    # Check if integration test script exists
    if [ ! -f "scratch/soft-ue-integration-test.cc" ]; then
        log_warning "Integration test script not found, skipping..."
        return 0
    fi

    # Run integration test
    log_info "Executing Soft-UE integration test..."

    if [ "$VERBOSE_OUTPUT" = true ]; then
        ./ns3 run "soft-ue-integration-test --verbose=true" \
            2>&1 | tee "$TEST_RESULTS_DIR/logs/integration-test.log"
        local result=${PIPESTATUS[0]}
    else
        ./ns3 run "soft-ue-integration-test" \
            > "$TEST_RESULTS_DIR/logs/integration-test.log" 2>&1
        local result=$?
    fi

    if [ $result -eq 0 ]; then
        log_success "✅ Integration test passed"
        return 0
    else
        log_error "❌ Integration test failed"
        return 1
    fi
}

# Run performance tests
run_performance_tests() {
    if [ "$ENABLE_PERFORMANCE_TEST" != true ]; then
        log_info "Performance tests disabled, skipping..."
        return 0
    fi

    log_info "Running performance tests..."

    cd "$PROJECT_ROOT"

    # Performance test configurations
    declare -a PERF_CONFIGS=(
        "--packets=1000 --size=512"
        "--packets=10000 --size=1500"
        "--packets=100000 --size=8192"
    )

    for config in "${PERF_CONFIGS[@]}"; do
        log_info "Testing with config: $config"

        local test_name="perf-$(echo $config | tr ' ' '-' | tr '=' '_')"

        if [ "$VERBOSE_OUTPUT" = true ]; then
            ./ns3 run "soft-ue-performance-test $config" \
                2>&1 | tee "$TEST_RESULTS_DIR/logs/${test_name}.log"
            local result=${PIPESTATUS[0]}
        else
            ./ns3 run "soft-ue-performance-test $config" \
                > "$TEST_RESULTS_DIR/logs/${test_name}.log" 2>&1
            local result=$?
        fi

        if [ $result -eq 0 ]; then
            log_success "✅ Performance test passed: $config"
        else
            log_error "❌ Performance test failed: $config"
        fi
    done
}

# Run stress tests
run_stress_tests() {
    if [ "$ENABLE_STRESS_TEST" != true ]; then
        log_info "Stress tests disabled, skipping..."
        return 0
    fi

    log_info "Running stress tests..."

    cd "$PROJECT_ROOT"

    # Stress test configurations
    declare -a STRESS_CONFIGS=(
        "--nodes=100 --duration=60"
        "--nodes=500 --duration=30"
        "--concurrency=1000 --packets=100000"
    )

    for config in "${STRESS_CONFIGS[@]}"; do
        log_info "Stress testing with config: $config"

        local test_name="stress-$(echo $config | tr ' ' '-' | tr '=' '_')"

        if [ "$VERBOSE_OUTPUT" = true ]; then
            ./ns3 run "soft-ue-stress-test $config" \
                2>&1 | tee "$TEST_RESULTS_DIR/logs/${test_name}.log"
            local result=${PIPESTATUS[0]}
        else
            ./ns3 run "soft-ue-stress-test $config" \
                > "$TEST_RESULTS_DIR/logs/${test_name}.log" 2>&1
            local result=$?
        fi

        if [ $result -eq 0 ]; then
            log_success "✅ Stress test passed: $config"
        else
            log_error "❌ Stress test failed: $config"
        fi
    done
}

# Generate coverage report
generate_coverage_report() {
    if [ "$ENABLE_COVERAGE" != true ]; then
        log_info "Coverage report generation disabled"
        return 0
    fi

    log_info "Generating code coverage report..."

    cd "$BUILD_DIR"

    # Check if gcov is available
    if ! command -v gcov &> /dev/null; then
        log_warning "gcov not found, skipping coverage report"
        return 0
    fi

    # Generate coverage data
    gcov -r ../src/soft-ue/ 2>/dev/null || true

    # Check if lcov is available for HTML report
    if command -v lcov &> /dev/null; then
        lcov --capture --directory . --output-file "$TEST_RESULTS_DIR/coverage/coverage.info" 2>/dev/null || true
        lcov --remove "$TEST_RESULTS_DIR/coverage/coverage.info" '/usr/*' --output-file "$TEST_RESULTS_DIR/coverage/coverage.info" 2>/dev/null || true

        if command -v genhtml &> /dev/null; then
            genhtml "$TEST_RESULTS_DIR/coverage/coverage.info" --output-directory "$TEST_RESULTS_DIR/coverage/html" 2>/dev/null || true
            log_success "✅ HTML coverage report generated: $TEST_RESULTS_DIR/coverage/html/index.html"
        fi
    fi

    # Display coverage summary
    if [ -f "$TEST_RESULTS_DIR/coverage/coverage.info" ]; then
        lcov --summary "$TEST_RESULTS_DIR/coverage/coverage.info" 2>/dev/null || true
    fi
}

# Generate test summary report
generate_test_report() {
    log_info "Generating test summary report..."

    local report_file="$TEST_RESULTS_DIR/test-summary-$TIMESTAMP.txt"

    cat > "$report_file" << EOF
Soft-UE Protocol Stack Test Summary Report
========================================
Generated: $(date)
Test Suite: Soft-UE ns-3 Implementation
Build Directory: $BUILD_DIR

Test Configuration:
- Coverage Analysis: $ENABLE_COVERAGE
- Performance Testing: $ENABLE_PERFORMANCE_TEST
- Stress Testing: $ENABLE_STRESS_TEST
- Verbose Output: $VERBOSE_OUTPUT

Test Results:
EOF

    # Count test results
    local total_logs=$(find "$TEST_RESULTS_DIR/logs" -name "*.log" | wc -l)
    local failed_logs=$(grep -l "FAILED\|ERROR" "$TEST_RESULTS_DIR/logs"/*.log 2>/dev/null | wc -l)
    local passed_logs=$((total_logs - failed_logs))

    echo "Total Test Cases: $total_logs" >> "$report_file"
    echo "Passed: $passed_logs" >> "$report_file"
    echo "Failed: $failed_logs" >> "$report_file"
    echo "Success Rate: $(( (passed_logs * 100) / total_logs ))%" >> "$report_file"

    echo "" >> "$report_file"
    echo "Detailed Results:" >> "$report_file"

    # Add detailed test results
    for log_file in "$TEST_RESULTS_DIR/logs"/*.log; do
        if [ -f "$log_file" ]; then
            local test_name=$(basename "$log_file" .log)
            local test_status="PASSED"

            if grep -q "FAILED\|ERROR\|failed" "$log_file" 2>/dev/null; then
                test_status="FAILED"
            fi

            echo "- $test_name: $test_status" >> "$report_file"
        fi
    done

    echo "" >> "$report_file"
    echo "Report Files:" >> "$report_file"
    echo "- Test Logs: $TEST_RESULTS_DIR/logs/" >> "$report_file"
    echo "- Coverage Report: $TEST_RESULTS_DIR/coverage/" >> "$report_file"
    echo "- Performance Data: $TEST_RESULTS_DIR/performance/" >> "$report_file"

    log_success "✅ Test summary report generated: $report_file"

    # Display summary
    echo ""
    echo "======================================================================"
    echo "                      Test Summary                                   "
    echo "======================================================================"
    echo "Total Tests: $total_logs"
    echo "Passed: $passed_logs"
    echo "Failed: $failed_logs"
    echo "Success Rate: $(( (passed_logs * 100) / total_logs ))%"
    echo ""
    echo "Detailed report: $report_file"
    if [ -f "$TEST_RESULTS_DIR/coverage/html/index.html" ]; then
        echo "Coverage report: $TEST_RESULTS_DIR/coverage/html/index.html"
    fi
    echo "======================================================================"
}

# Cleanup function
cleanup() {
    log_info "Cleaning up temporary files..."
    # Add any cleanup tasks here
}

# Main execution function
main() {
    # Set up trap for cleanup
    trap cleanup EXIT

    print_header

    # Check command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --no-coverage)
                ENABLE_COVERAGE=false
                shift
                ;;
            --no-performance)
                ENABLE_PERFORMANCE_TEST=false
                shift
                ;;
            --no-stress)
                ENABLE_STRESS_TEST=false
                shift
                ;;
            --quiet)
                VERBOSE_OUTPUT=false
                shift
                ;;
            --help|-h)
                echo "Usage: $0 [OPTIONS]"
                echo ""
                echo "Options:"
                echo "  --no-coverage     Disable code coverage analysis"
                echo "  --no-performance  Disable performance testing"
                echo "  --no-stress       Disable stress testing"
                echo "  --quiet           Reduce output verbosity"
                echo "  --help, -h        Show this help message"
                exit 0
                ;;
            *)
                log_error "Unknown option: $1"
                echo "Use --help for usage information"
                exit 1
                ;;
        esac
    done

    # Execute test phases
    local overall_result=0

    check_build || overall_result=1

    run_unit_tests || overall_result=1

    run_integration_tests || overall_result=1

    run_performance_tests || overall_result=1

    run_stress_tests || overall_result=1

    generate_coverage_report

    generate_test_report

    # Exit with appropriate code
    if [ $overall_result -eq 0 ]; then
        log_success "🎉 All tests completed successfully!"
        exit 0
    else
        log_error "💥 Some tests failed. Check the logs for details."
        exit 1
    fi
}

# Execute main function
main "$@"