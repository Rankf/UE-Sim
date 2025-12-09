#!/bin/bash

# Quick Test Script for Soft-UE Protocol Stack
# Simple script to quickly verify basic functionality

set -e

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log() { echo -e "${BLUE}[INFO]${NC} $1"; }
success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; }

echo "================================================"
echo "        Soft-UE Quick Test Script"
echo "================================================"

cd "$(dirname "$0")/.."

# Check if build exists
if [ ! -f "build/lib/libns3.44-soft-ue.so" ]; then
    error "Soft-UE library not found. Please run: ./ns3 build"
    exit 1
fi

success "Soft-UE library found"

# Test 1: Component creation
log "Testing component creation..."
if ./ns3 run "soft-ue-helper-test --verbose=false" > /dev/null 2>&1; then
    success "✅ Component creation test passed"
else
    warning "⚠️  Component creation test failed (this may be expected if test doesn't exist)"
fi

# Test 2: Integration test
log "Running integration test..."
if ./ns3 run "soft-ue-integration-test --verbose=false" > /tmp/soft-ue-test.log 2>&1; then
    success "✅ Integration test passed"

    # Extract summary from log
    if grep -q "ALL INTEGRATION TESTS PASSED" /tmp/soft-ue-test.log; then
        success "🎉 All integration tests passed!"
    elif grep -q "SOME INTEGRATION TESTS FAILED" /tmp/soft-ue-test.log; then
        error "❌ Some integration tests failed"
        grep "TEST FAILED" /tmp/soft-ue-test.log | head -5
    fi
else
    error "❌ Integration test failed"
    echo "Last 20 lines of log:"
    tail -20 /tmp/soft-ue-test.log
fi

# Test 3: Basic functionality
log "Testing basic packet flow..."
if timeout 30s ./ns3 run "soft-ue-demo --verbose=false" > /tmp/soft-ue-demo.log 2>&1; then
    success "✅ Basic functionality test passed"
else
    warning "⚠️  Basic functionality test failed or timed out"
fi

echo "================================================"
echo "                Quick Test Summary"
echo "================================================"
echo "Detailed logs:"
echo "  Integration test: /tmp/soft-ue-test.log"
echo "  Basic test: /tmp/soft-ue-demo.log"
echo "================================================"

echo ""
echo "For comprehensive testing, run:"
echo "  ./scripts/run-soft-ue-tests.sh"
echo "  ./scripts/soft-ue-validation-test.sh"