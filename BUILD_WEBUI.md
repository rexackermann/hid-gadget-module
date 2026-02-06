# Building WebUI for Android

## Problem
The WebUI binary requires OpenSSL, which cannot be statically cross-compiled with Zig from a Linux host. The binary needs to be compiled natively on Android.

## Solution: Build in Termux

### Step 1: Install Termux
Download from F-Droid or GitHub releases

### Step 2: Copy Project to Android
```bash
# From your computer
adb push /path/to/hid-gadget-module /sdcard/
```

### Step 3: Build in Termux
```bash
# In Termux
cd /sdcard/hid-gadget-module
chmod +x build-webui-termux.sh
./build-webui-termux.sh
```

This will:
1. Install `clang` and `openssl` packages
2. Compile `hid-webui` for ARM64 with dynamic OpenSSL linking
3. Create a working ARM64 binary

### Step 4: Test
```bash
chmod +x hid-webui
./hid-webui
# Open http://localhost:8080
```

### Step 5: Package (Optional)
```bash
# Copy binary to system/bin
cp hid-webui system/bin/hid-webui-bin

# Pull back to computer
adb pull /sdcard/hid-gadget-module/system/bin/hid-webui-bin .

# On computer: regenerate ZIP
zip -r hid-gadget-module-v1.40.1.zip blobs/ system/ META-INF/ module.prop service.sh customize.sh webui/ -x "*.git*" -x "src/*" -x "include/*" -x "tests/*" -x "scripts/*"
```

---

## Alternative: Pre-compiled Binary

I can provide a pre-compiled ARM64 binary if you prefer not to build in Termux. However, building natively ensures compatibility with your specific Android version and OpenSSL library.

---

## Why This Happened

- `hid-gadget` doesn't use OpenSSL → Can be statically cross-compiled with Zig ✅
- `hid-webui` uses OpenSSL for WebSocket handshake → Needs dynamic linking ❌
- Zig can't find ARM64 OpenSSL libraries for cross-compilation
- Native compilation in Termux solves this

---

## Future Fix

We could:
1. Implement standalone SHA1 (no OpenSSL dependency)
2. Use a different WebSocket library
3. Bundle static OpenSSL libraries for cross-compilation

For now, Termux compilation is the fastest solution.
