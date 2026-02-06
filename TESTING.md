# Quick Testing Guide for WebUI

## ✅ Fixed Issues
- **Permission denied**: Fixed `hid-webui` wrapper script
- **Binary execution**: Now properly calls `hid-webui-bin`
- **Path detection**: Auto-detects local vs installed mode

---

## 🧪 Testing Without Installation

### Method 1: Using test-webui.sh (Recommended)
```bash
# Copy entire project directory to phone
adb push /path/to/hid-gadget-module /sdcard/

# On phone (via Termux or ADB shell)
cd /sdcard/hid-gadget-module
chmod +x test-webui.sh hid-webui
./test-webui.sh
```

### Method 2: Direct execution
```bash
cd /sdcard/hid-gadget-module
chmod +x hid-webui
./hid-webui
```

Both methods will:
- Auto-detect you're running from project directory
- Use local `./hid-webui` binary
- Serve from local `./webui/` directory
- Start server on `http://localhost:8080`

---

## 📦 After Installation (Magisk/KernelSU)

Once you flash the module, it will use system paths:
```bash
hid-webui
# Uses: /system/bin/hid-webui-bin
# Serves: /data/adb/modules/hid-gadget/webui/
```

---

## 🔍 Troubleshooting

**"Permission denied"**
```bash
chmod +x hid-webui test-webui.sh
```

**"Binary not found"**
- Make sure `hid-webui` (the binary) exists in project root
- Check: `ls -la hid-webui`

**"WebUI directory not found"**
- Ensure `webui/` directory exists with HTML/CSS/JS files
- Check: `ls -la webui/`

**"Can't bind to port 8080"**
- Port already in use
- Kill existing process: `pkill hid-webui`

---

## 📱 Accessing the Interface

**Same Device:**
```
http://localhost:8080
```

**From Network:**
```bash
# Find your IP
ip addr show wlan0 | grep 'inet '

# Access from browser
http://<your-ip>:8080
```

---

## 🎯 What's Fixed

1. ✅ `hid-webui` wrapper now executable
2. ✅ Properly calls `hid-webui-bin` binary
3. ✅ Auto-detects local vs installed paths
4. ✅ Better error messages
5. ✅ Test script for easy testing
6. ✅ Regenerated flashable ZIP with fixes
