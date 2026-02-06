#!/bin/bash

# WebUI Integration Test Script
# Automated testing for HID WebSocket server

set -e

# Colors
RED='\033[1;31m'
GREEN='\033[1;32m'
YELLOW='\033[1;33m'
BLUE='\033[1;36m'
MAGENTA='\033[1;35m'
RESET='\033[0m'

# Configuration
SERVER_BIN="./hid-webui"
TEST_BIN="./tests/webui_test"
SERVER_PID=""
TEST_TIMEOUT=30

# Cleanup function
cleanup() {
    if [ -n "$SERVER_PID" ]; then
        echo -e "${YELLOW}[INFO]${RESET} Stopping server (PID: $SERVER_PID)..."
        kill $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
    fi
}

trap cleanup EXIT

echo ""
echo -e "${MAGENTA}╔════════════════════════════════════════════════════════╗${RESET}"
echo -e "${MAGENTA}║     HID WebUI - Integration Test Suite                ║${RESET}"
echo -e "${MAGENTA}╚════════════════════════════════════════════════════════╝${RESET}"
echo ""

# Check if binaries exist
if [ ! -f "$SERVER_BIN" ]; then
    echo -e "${RED}[ERROR]${RESET} Server binary not found: $SERVER_BIN"
    echo -e "${YELLOW}[INFO]${RESET} Run 'make hid-webui' to build the server"
    exit 1
fi

if [ ! -f "$TEST_BIN" ]; then
    echo -e "${RED}[ERROR]${RESET} Test binary not found: $TEST_BIN"
    echo -e "${YELLOW}[INFO]${RESET} Run 'make test-webui' to build the test suite"
    exit 1
fi

# Start server
echo -e "${BLUE}[1/4]${RESET} Starting WebSocket server..."
$SERVER_BIN &
SERVER_PID=$!

# Wait for server to start
echo -e "${BLUE}[2/4]${RESET} Waiting for server to initialize..."
sleep 2

# Check if server is running
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo -e "${RED}[ERROR]${RESET} Server failed to start"
    exit 1
fi

echo -e "${GREEN}[OK]${RESET} Server running (PID: $SERVER_PID)"
echo ""

# Run tests
echo -e "${BLUE}[3/4]${RESET} Running automated test suite..."
echo ""

if $TEST_BIN; then
    TEST_RESULT=0
else
    TEST_RESULT=$?
fi

echo ""

# Generate report
echo -e "${BLUE}[4/4]${RESET} Generating test report..."
echo ""

if [ $TEST_RESULT -eq 0 ]; then
    echo -e "${GREEN}╔════════════════════════════════════════════════════════╗${RESET}"
    echo -e "${GREEN}║              ✓ ALL TESTS PASSED                        ║${RESET}"
    echo -e "${GREEN}╚════════════════════════════════════════════════════════╝${RESET}"
    echo ""
    echo -e "${GREEN}[SUCCESS]${RESET} WebUI is ready for deployment!"
    echo ""
    exit 0
else
    echo -e "${RED}╔════════════════════════════════════════════════════════╗${RESET}"
    echo -e "${RED}║              ✗ TESTS FAILED                            ║${RESET}"
    echo -e "${RED}╚════════════════════════════════════════════════════════╝${RESET}"
    echo ""
    echo -e "${RED}[FAILURE]${RESET} Please review the test output above"
    echo ""
    exit 1
fi
