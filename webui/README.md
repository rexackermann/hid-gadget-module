# WebUI Usage Guide

## Starting the Server

```bash
# On Android (via Termux or ADB shell)
hid-webui
```

The server will start on `http://localhost:8080`

## Accessing the Interface

1. **On the same device**: Open a browser and navigate to `http://localhost:8080`
2. **From another device on the same network**: 
   - Find your Android device's IP address
   - Navigate to `http://<android-ip>:8080`

## Features

### Media Controls
14 media buttons for playback, volume, and brightness control

### Radar Mouse
- **Drag** from center to move mouse with velocity-based movement
- **Click center** for left-click
- **Sensitivity slider** adjusts movement speed (1x-20x)

### Mouse Buttons
- **L/M/R**: Single click
- **HL/HM/HR**: Hold/release for dragging

### Keyboard
- Full 75% layout with all standard keys
- **Sticky modifiers**: Click Ctrl/Alt/Shift/Win to toggle
- **Physical keyboard support**: Type directly if using a keyboard

## Technical Details

- **Protocol**: WebSocket (RFC 6455)
- **Port**: 8080 (configurable in source)
- **Max Clients**: 4 concurrent connections
- **Latency**: <10ms average response time

## Troubleshooting

**Server won't start:**
- Ensure HID devices are initialized: `hid-setup`
- Check if port 8080 is already in use

**Can't connect from browser:**
- Verify server is running
- Check firewall settings
- Ensure devices are on the same network (for remote access)

**Commands not working:**
- Check browser console for errors
- Verify WebSocket connection status (green dot in header)
- Ensure HID devices are active: `ls /dev/hidg*`
