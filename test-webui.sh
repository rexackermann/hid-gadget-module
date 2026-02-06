#!/bin/bash

# Quick Test Script for HID WebUI
# Copy this entire directory to your phone and run this script

echo "=========================================="
echo "  HID WebUI - Quick Test"
echo "=========================================="
echo ""

# Check if we're in the right directory
if [ ! -f "hid-webui" ] || [ ! -d "webui" ]; then
    echo "❌ Error: Run this script from the hid-gadget-module directory"
    echo "   Expected files: hid-webui, webui/"
    exit 1
fi

# Make binaries executable
chmod +x hid-webui system/bin/hid-webui 2>/dev/null

echo "✅ Starting WebUI server..."
echo ""
echo "📱 Access the interface:"
echo "   - Same device: http://localhost:8080"
echo "   - Network: http://$(ip addr show wlan0 2>/dev/null | grep 'inet ' | awk '{print $2}' | cut -d/ -f1):8080"
echo ""
echo "Press Ctrl+C to stop"
echo ""

# Run the server
./hid-webui
