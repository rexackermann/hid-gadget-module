# ARM64 Cross-Compilation Setup for WebUI

## Required Dependencies

### For Fedora/RHEL/CentOS
```bash
sudo dnf install -y \
    gcc-aarch64-linux-gnu \
    openssl-devel \
    glibc-aarch64-linux-gnu
```

### For Debian/Ubuntu
```bash
# Add arm64 architecture
sudo dpkg --add-architecture arm64
sudo apt-get update

# Install cross-compilation tools
sudo apt-get install -y \
    gcc-aarch64-linux-gnu \
    libssl-dev:arm64 \
    libc6-dev:arm64
```

### For Arch Linux
```bash
sudo pacman -S aarch64-linux-gnu-gcc
```

## Building WebUI

Once dependencies are installed:

```bash
# Using GCC cross-compiler
aarch64-linux-gnu-gcc -Wall -Wextra -O2 -Iinclude -o hid-webui src/webui.c -lssl -lcrypto

# Verify it's ARM64
file hid-webui
# Should show: ELF 64-bit LSB executable, ARM aarch64
```

## Troubleshooting

**Error: "arpa/inet.h: No such file or directory"**
- Install: `libc6-dev:arm64` (Debian/Ubuntu) or `glibc-aarch64-linux-gnu` (Fedora)

**Error: "cannot find -lssl"**
- Install: `libssl-dev:arm64` (Debian/Ubuntu) or `openssl-devel` (Fedora)

**Error: "Exec format error" when running**
- Binary is x86_64, not ARM64
- Check with: `file hid-webui`
- Rebuild with correct cross-compiler

## Alternative: Use Zig (if OpenSSL not needed)

If we remove OpenSSL dependency:
```bash
zig cc --target=aarch64-linux-musl -static -Wall -Wextra -O2 -Iinclude -o hid-webui src/webui.c
```

This requires implementing standalone SHA1 for WebSocket handshake.
