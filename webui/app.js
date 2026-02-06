// WebSocket connection
let ws = null;
let reconnectInterval = null;
const WS_URL = 'ws://' + window.location.host;

// State management
const state = {
    connected: false,
    stickyModifiers: new Set(),
    heldButtons: new Set(),
    sensitivity: 5,
    radarActive: false,
    radarStartX: 0,
    radarStartY: 0
};

// Initialize WebSocket connection
function connect() {
    ws = new WebSocket(WS_URL);

    ws.onopen = () => {
        console.log('Connected to HID server');
        state.connected = true;
        updateStatus(true);
        clearInterval(reconnectInterval);
    };

    ws.onclose = () => {
        console.log('Disconnected from HID server');
        state.connected = false;
        updateStatus(false);

        // Auto-reconnect
        if (!reconnectInterval) {
            reconnectInterval = setInterval(() => {
                console.log('Attempting to reconnect...');
                connect();
            }, 3000);
        }
    };

    ws.onerror = (error) => {
        console.error('WebSocket error:', error);
    };

    ws.onmessage = (event) => {
        try {
            const data = JSON.parse(event.data);
            handleServerMessage(data);
        } catch (e) {
            console.error('Failed to parse message:', e);
        }
    };
}

// Send command to server
function sendCommand(type, data) {
    if (!state.connected || !ws || ws.readyState !== WebSocket.OPEN) {
        console.warn('Not connected to server');
        return;
    }

    const message = {
        type: type,
        ...data
    };

    ws.send(JSON.stringify(message));
}

// Handle server messages
function handleServerMessage(data) {
    console.log('Server message:', data);
    // Handle status updates, errors, etc.
}

// Update connection status UI
function updateStatus(connected) {
    const statusDot = document.querySelector('.status-dot');
    const statusText = document.getElementById('status-text');

    if (connected) {
        statusDot.classList.add('connected');
        statusText.textContent = 'Connected';
    } else {
        statusDot.classList.remove('connected');
        statusText.textContent = 'Disconnected';
    }
}

// Media controls
document.querySelectorAll('.media-btn').forEach(btn => {
    btn.addEventListener('click', () => {
        const action = btn.dataset.action;
        sendCommand('consumer', { action });

        // Visual feedback
        btn.style.transform = 'scale(0.95)';
        setTimeout(() => {
            btn.style.transform = '';
        }, 100);
    });
});

// Mouse controls - Radar
const radar = document.getElementById('radar');
const ctx = radar.getContext('2d');
const radarRadius = radar.width / 2;

function drawRadar(x, y) {
    ctx.clearRect(0, 0, radar.width, radar.height);

    // Draw concentric circles
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.2)';
    ctx.lineWidth = 1;
    for (let i = 1; i <= 3; i++) {
        ctx.beginPath();
        ctx.arc(radarRadius, radarRadius, (radarRadius / 3) * i, 0, Math.PI * 2);
        ctx.stroke();
    }

    // Draw crosshair
    ctx.beginPath();
    ctx.moveTo(radarRadius, 0);
    ctx.lineTo(radarRadius, radar.height);
    ctx.moveTo(0, radarRadius);
    ctx.lineTo(radar.width, radarRadius);
    ctx.stroke();

    // Draw current position
    if (x !== null && y !== null) {
        ctx.fillStyle = 'rgba(100, 200, 255, 0.8)';
        ctx.beginPath();
        ctx.arc(x, y, 8, 0, Math.PI * 2);
        ctx.fill();

        // Draw line from center
        ctx.strokeStyle = 'rgba(100, 200, 255, 0.6)';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(radarRadius, radarRadius);
        ctx.lineTo(x, y);
        ctx.stroke();
    }
}

// Initialize radar
drawRadar(null, null);

// Radar mouse events
radar.addEventListener('mousedown', (e) => handleRadarStart(e));
radar.addEventListener('mousemove', (e) => handleRadarMove(e));
radar.addEventListener('mouseup', () => handleRadarEnd());
radar.addEventListener('mouseleave', () => handleRadarEnd());

// Touch events for mobile
radar.addEventListener('touchstart', (e) => {
    e.preventDefault();
    handleRadarStart(e.touches[0]);
});
radar.addEventListener('touchmove', (e) => {
    e.preventDefault();
    handleRadarMove(e.touches[0]);
});
radar.addEventListener('touchend', () => handleRadarEnd());

function handleRadarStart(e) {
    const rect = radar.getBoundingClientRect();
    state.radarActive = true;
    state.radarStartX = e.clientX - rect.left;
    state.radarStartY = e.clientY - rect.top;

    // Check if it's a center click
    const dx = state.radarStartX - radarRadius;
    const dy = state.radarStartY - radarRadius;
    const distance = Math.sqrt(dx * dx + dy * dy);

    if (distance < 30) {
        // Center click = left click
        sendCommand('mouse', { action: 'click', button: 'left' });
    }
}

function handleRadarMove(e) {
    if (!state.radarActive) return;

    const rect = radar.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;

    // Calculate delta from center
    const dx = x - radarRadius;
    const dy = y - radarRadius;
    const distance = Math.sqrt(dx * dx + dy * dy);

    // Normalize and apply sensitivity
    if (distance > 10) {
        const maxDistance = radarRadius * 0.8;
        const normalizedDistance = Math.min(distance, maxDistance) / maxDistance;
        const moveX = Math.round((dx / distance) * normalizedDistance * state.sensitivity * 20);
        const moveY = Math.round((dy / distance) * normalizedDistance * state.sensitivity * 20);

        sendCommand('mouse', { action: 'move', x: moveX, y: moveY });
        drawRadar(x, y);
    }
}

function handleRadarEnd() {
    state.radarActive = false;
    drawRadar(null, null);
}

// Mouse click buttons
document.querySelectorAll('.mouse-btn:not(.hold-btn)').forEach(btn => {
    btn.addEventListener('click', () => {
        const action = btn.dataset.action;
        const button = action.split('-')[1]; // click-left -> left
        sendCommand('mouse', { action: 'click', button });

        // Visual feedback
        btn.classList.add('pressed');
        setTimeout(() => btn.classList.remove('pressed'), 200);
    });
});

// Mouse hold buttons
document.querySelectorAll('.hold-btn').forEach(btn => {
    btn.addEventListener('click', () => {
        const action = btn.dataset.action;
        const button = action.split('-')[1]; // hold-left -> left

        if (state.heldButtons.has(button)) {
            // Release
            state.heldButtons.delete(button);
            btn.classList.remove('active');
            sendCommand('mouse', { action: 'up', button });
        } else {
            // Hold
            state.heldButtons.add(button);
            btn.classList.add('active');
            sendCommand('mouse', { action: 'down', button });
        }
    });
});

// Sensitivity slider
const sensitivitySlider = document.getElementById('sensitivity');
const sensValue = document.getElementById('sens-value');

sensitivitySlider.addEventListener('input', (e) => {
    state.sensitivity = parseInt(e.target.value);
    sensValue.textContent = state.sensitivity;
});

// Keyboard handling
document.querySelectorAll('.key').forEach(btn => {
    const key = btn.dataset.key;
    const isModifier = btn.classList.contains('modifier-key');
    const isSticky = btn.classList.contains('sticky');

    btn.addEventListener('click', () => {
        if (isSticky) {
            // Toggle sticky modifier
            if (state.stickyModifiers.has(key)) {
                state.stickyModifiers.delete(key);
                btn.classList.remove('active');
            } else {
                state.stickyModifiers.add(key);
                btn.classList.add('active');
            }
        } else {
            // Regular key press
            const modifiers = Array.from(state.stickyModifiers);
            sendCommand('keyboard', {
                action: 'press',
                key,
                modifiers
            });

            // Clear sticky modifiers after use (unless it's another modifier)
            if (!isModifier) {
                state.stickyModifiers.forEach(mod => {
                    const modBtn = document.querySelector(`.key[data-key="${mod}"]`);
                    if (modBtn) modBtn.classList.remove('active');
                });
                state.stickyModifiers.clear();
            }

            // Visual feedback
            btn.classList.add('pressed');
            setTimeout(() => btn.classList.remove('pressed'), 200);
        }
    });
});

// Physical keyboard support
document.addEventListener('keydown', (e) => {
    // Prevent default for special keys
    if (['Tab', 'Space', 'ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight'].includes(e.key)) {
        e.preventDefault();
    }

    let key = e.key.toUpperCase();

    // Map special keys
    const keyMap = {
        ' ': 'SPACE',
        'ARROWUP': 'UP',
        'ARROWDOWN': 'DOWN',
        'ARROWLEFT': 'LEFT',
        'ARROWRIGHT': 'RIGHT',
        'CONTROL': 'CTRL',
        'META': 'GUI'
    };

    if (keyMap[key]) {
        key = keyMap[key];
    }

    // Find and trigger the button
    const btn = document.querySelector(`.key[data-key="${key}"]`);
    if (btn && !btn.classList.contains('pressed')) {
        btn.click();
    }
});

// Initialize connection on load
window.addEventListener('load', () => {
    connect();
});

// Cleanup on unload
window.addEventListener('beforeunload', () => {
    if (ws) {
        ws.close();
    }
});
