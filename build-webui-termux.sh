#!/data/data/com.termux/files/usr/bin/bash

# Build script for WebUI on Android (Termux)
# Run this script in Termux to compile hid-webui natively

echo "=========================================="
echo "  Building HID WebUI on Android"
echo "=========================================="
echo ""

# Check if we're in Termux
if [ ! -d "/data/data/com.termux" ]; then
    echo "❌ Error: This script must be run in Termux"
    exit 1
fi

# Install dependencies
echo "📦 Installing dependencies..."
pkg install -y clang openssl || {
    echo "❌ Failed to install dependencies"
    exit 1
}

# Navigate to project directory
if [ ! -f "src/webui.c" ]; then
    echo "❌ Error: Run this script from the hid-gadget-module directory"
    exit 1
fi

# Compile WebUI
echo ""
echo "🔨 Compiling hid-webui..."
clang -Wall -Wextra -O2 -Iinclude -o hid-webui src/webui.c -lssl -lcrypto || {
    echo "❌ Compilation failed"
    exit 1
}

# Verify binary
echo ""
echo "✅ Build successful!"
file hid-webui
ls -lh hid-webui

echo ""
echo "📝 To test:"
echo "   chmod +x hid-webui"
echo "   ./hid-webui"
echo ""
echo "📦 To package:"
echo "   cp hid-webui system/bin/hid-webui-bin"
echo "   # Then create ZIP on your computer"
