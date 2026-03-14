// --- Get elements ---
const mainPage = document.getElementById('main-page');
const settingsPage = document.getElementById('settings-page');
const userManagementPage = document.getElementById('user-management-page');
const loginModal = document.getElementById('login-modal');
const enableDisplay = document.getElementById('enable-display');
const timeElement = document.getElementById('time');
const ntpStatusElement = document.getElementById('ntp-status');
const roundCounterElement = document.getElementById('round-counter');
const mainTimerDisplay = document.getElementById('main-timer-display');
const startBtn = document.getElementById('start-btn');
const pauseBtn = document.getElementById('pause-btn');
const pauseAfterNextBtn = document.getElementById('pause-after-next-btn');
const resetBtn = document.getElementById('reset-btn');
const settingsIcon = document.getElementById('settings-icon');
const userIcon = document.getElementById('user-icon');
const qrIcon = document.getElementById('qr-icon');
const saveSettingsBtn = document.getElementById('save-settings-btn');

// --- Login elements ---
const loginUsernameInput = document.getElementById('login-username');
const loginPasswordInput = document.getElementById('login-password');
const loginBtn = document.getElementById('login-btn');
const viewerBtn = document.getElementById('viewer-btn');
const loginDifferentBtn = document.getElementById('login-different-btn');
const currentUserInfo = document.getElementById('current-user-info');

// --- User management elements ---
const newOperatorUsernameInput = document.getElementById('new-operator-username');
const newOperatorPasswordInput = document.getElementById('new-operator-password');
const addOperatorBtn = document.getElementById('add-operator-btn');
const operatorsList = document.getElementById('operators-list');
const changeOldPasswordInput = document.getElementById('change-old-password');
const changeNewPasswordInput = document.getElementById('change-new-password');
const changePasswordBtn = document.getElementById('change-password-btn');
const factoryResetBtn = document.getElementById('factory-reset-btn');
const closeUserManagementBtn = document.getElementById('close-user-management-btn');

// --- Help elements ---
const helpPage = document.getElementById('help-page');
const helpIcon = document.getElementById('help-icon');
const closeHelpBtn = document.getElementById('close-help-btn');

// --- QR elements ---
const qrPage = document.getElementById('qr-page');
const qrCloseBtn = document.getElementById('qr-close-btn');
const qrWifiCanvas = document.getElementById('qr-wifi-canvas');
const qrAppCanvas = document.getElementById('qr-app-canvas');

// --- Settings inputs ---
const gameDurationInput = document.getElementById('game-duration');
const numRoundsInput = document.getElementById('num-rounds');
const sirenLengthInput = document.getElementById('siren-length');
const sirenPauseInput = document.getElementById('siren-pause');

// --- Upcoming events elements ---
const upcomingEventsList = document.getElementById('upcoming-events-list');
const syncNowBtn = document.getElementById('sync-now-btn');
const lastSyncInfo = document.getElementById('last-sync-info');

// --- Event window elements ---
const eventWindowInfo = document.getElementById('event-window-info');
const eventNameDisplay = document.getElementById('event-name-display');
const eventEndDisplay = document.getElementById('event-end-display');
const pauseAfterNextBanner = document.getElementById('pause-after-next-banner');

// --- State ---
const SIMULATION_MODE = window.location.protocol.startsWith('file');
let socket;
let settings = {};

// Client-side timer state — smooth interpolation
let displayEndTime = 0;    // Absolute timestamp when timer should reach zero (client clock)
let serverEndTime = 0;     // Target end time from last server sync
let isClientTimerPaused = false;
let animationFrameId = null;
let lastFrameTime = 0;

// Client-side clock — ticks locally between server syncs
let serverTimeOffset = 0;  // Difference: serverTime - clientTime (ms)

// Reconnection
let reconnectAttempts = 0;
const MAX_RECONNECT_ATTEMPTS = 10;
let reconnectTimeout = null;

// Auth
let userRole = 'viewer';
let currentUsername = 'Viewer';

// Timer state
let currentTimerStatus = 'IDLE';
let pauseAfterNextActive = false;
let continuousMode = false;

// Sound
let soundEnabled = true;
let audioContext = null;

// Button debounce
let buttonsEnabled = true;

// --- Utility Functions ---

function escapeHtml(text) {
    if (!text) return '';
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

function playBeep(frequency = 440, duration = 100) {
    if (!soundEnabled) return;
    if (!audioContext) {
        audioContext = new (window.AudioContext || window.webkitAudioContext)();
    }
    const oscillator = audioContext.createOscillator();
    const gainNode = audioContext.createGain();
    oscillator.connect(gainNode);
    gainNode.connect(audioContext.destination);
    oscillator.frequency.value = frequency;
    gainNode.gain.setValueAtTime(0.3, audioContext.currentTime);
    gainNode.gain.exponentialRampToValueAtTime(0.01, audioContext.currentTime + duration / 1000);
    oscillator.start();
    oscillator.stop(audioContext.currentTime + duration / 1000);
}

function showLoadingOverlay(message = "Loading...") {
    let overlay = document.getElementById('loading-overlay');
    if (!overlay) {
        overlay = document.createElement('div');
        overlay.id = 'loading-overlay';
        overlay.style.cssText = `position:fixed;top:0;left:0;width:100%;height:100%;
            background:rgba(0,0,0,0.7);display:flex;flex-direction:column;align-items:center;
            justify-content:center;z-index:10000;`;
        document.body.appendChild(overlay);
    }
    overlay.innerHTML = `
        <div class="spinner" style="border:8px solid #f3f3f3;border-top:8px solid #28a745;
            border-radius:50%;width:60px;height:60px;animation:spin 1s linear infinite;"></div>
        <p style="color:white;font-size:1.2em;margin-top:20px;">${message}</p>
        <style>@keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }</style>
    `;
    overlay.style.display = 'flex';
}

function hideLoadingOverlay() {
    const overlay = document.getElementById('loading-overlay');
    if (overlay) overlay.style.display = 'none';
}

function disableButtonsTemporarily(duration = 500) {
    if (!buttonsEnabled) return;
    buttonsEnabled = false;
    startBtn.disabled = true;
    pauseBtn.disabled = true;
    resetBtn.disabled = true;
    setTimeout(() => {
        buttonsEnabled = true;
        startBtn.disabled = false;
        pauseBtn.disabled = false;
        resetBtn.disabled = false;
    }, duration);
}

function formatTime(milliseconds) {
    if (isNaN(milliseconds) || milliseconds < 0) milliseconds = 0;
    const totalSeconds = Math.floor(milliseconds / 1000);
    const minutes = Math.floor(totalSeconds / 60);
    const seconds = totalSeconds % 60;
    return `${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`;
}

function formatRoundCounter(current, total) {
    return continuousMode ? `Round ${current}` : `Round ${current} of ${total}`;
}

function showTemporaryMessage(msg, type = "success") {
    const banner = document.createElement('div');
    banner.textContent = msg;
    banner.className = `temp-message temp-message-${type}`;
    banner.style.cssText = `position:fixed;top:20px;left:50%;transform:translateX(-50%);
        background:${type === 'error' ? '#dc3545' : '#28a745'};color:white;padding:12px 24px;
        border-radius:6px;z-index:10000;box-shadow:0 4px 8px rgba(0,0,0,0.3);
        font-weight:bold;animation:fadeIn 0.3s ease-in;max-width:90%;text-align:center;`;
    document.body.appendChild(banner);
    setTimeout(() => {
        banner.style.animation = 'fadeOut 0.3s ease-out';
        setTimeout(() => banner.remove(), 300);
    }, 3000);
}

// --- Timer Display ---

function clientTimerLoop() {
    if (isClientTimerPaused) {
        animationFrameId = requestAnimationFrame(clientTimerLoop);
        return;
    }

    const now = Date.now();

    // Smoothly steer displayEndTime toward serverEndTime
    // Converge ~3% per frame (~60fps = corrects 1s drift in ~0.6s)
    const error = serverEndTime - displayEndTime;
    if (Math.abs(error) > 50) {
        displayEndTime += error * 0.03;
    } else {
        displayEndTime = serverEndTime;
    }

    let remaining = displayEndTime - now;
    if (remaining < 0) remaining = 0;

    mainTimerDisplay.textContent = formatTime(remaining);

    if (remaining > 0) {
        animationFrameId = requestAnimationFrame(clientTimerLoop);
    }
}

function stopClientTimer() {
    if (animationFrameId) {
        cancelAnimationFrame(animationFrameId);
        animationFrameId = null;
    }
}

function startClientTimer() {
    stopClientTimer();
    if (!isClientTimerPaused) {
        animationFrameId = requestAnimationFrame(clientTimerLoop);
    }
}

// --- Page Navigation ---

function showSettingsPage(show) {
    mainPage.classList.toggle('hidden', show);
    settingsPage.classList.toggle('hidden', !show);
    userManagementPage.classList.add('hidden');
    if (helpPage) helpPage.classList.add('hidden');
    if (qrPage) qrPage.classList.add('hidden');
    if (show && userRole === 'admin') {
        sendWebSocketMessage({ action: 'get_helloclub_settings' });
    }
}

function showUserManagementPage(show) {
    mainPage.classList.toggle('hidden', show);
    settingsPage.classList.add('hidden');
    userManagementPage.classList.toggle('hidden', !show);
    if (helpPage) helpPage.classList.add('hidden');
    if (qrPage) qrPage.classList.add('hidden');
}

function showHelpPage(show) {
    mainPage.classList.toggle('hidden', show);
    settingsPage.classList.add('hidden');
    userManagementPage.classList.add('hidden');
    if (helpPage) helpPage.classList.toggle('hidden', !show);
    if (qrPage) qrPage.classList.add('hidden');
}

// --- UI Updates ---

function updateUIForRole() {
    document.body.className = `role-${userRole}`;
    if (currentUserInfo) {
        let roleDisplay = userRole.charAt(0).toUpperCase() + userRole.slice(1);
        currentUserInfo.innerHTML = `
            <strong>${escapeHtml(currentUsername)}</strong>
            <span class="badge badge-${userRole}">${roleDisplay}</span>
        `;
    }
    updatePauseAfterNextVisibility();
}

function updatePauseBtnLabel(paused) {
    if (!pauseBtn) return;
    const span = pauseBtn.querySelector('span');
    const svg = pauseBtn.querySelector('svg');
    if (paused) {
        if (span) span.textContent = 'Unpause';
        // Play icon (triangle)
        if (svg) svg.innerHTML = '<polygon points="5 3 19 12 5 21"></polygon>';
    } else {
        if (span) span.textContent = 'Pause';
        // Pause icon (two bars)
        if (svg) svg.innerHTML = '<rect x="6" y="4" width="4" height="16"></rect><rect x="14" y="4" width="4" height="16"></rect>';
    }
}

function updatePauseAfterNextVisibility() {
    if (!pauseAfterNextBtn) return;
    // Show only for operator+ when timer is RUNNING
    if ((userRole === 'operator' || userRole === 'admin') && currentTimerStatus === 'RUNNING') {
        pauseAfterNextBtn.classList.remove('hidden');
    } else {
        pauseAfterNextBtn.classList.add('hidden');
    }
}

function updatePauseAfterNextUI(active) {
    pauseAfterNextActive = active;
    if (pauseAfterNextBtn) {
        if (active) {
            pauseAfterNextBtn.classList.add('btn-warning-active');
            pauseAfterNextBtn.classList.remove('btn-outline-warning');
        } else {
            pauseAfterNextBtn.classList.remove('btn-warning-active');
            pauseAfterNextBtn.classList.add('btn-outline-warning');
        }
    }
    if (pauseAfterNextBanner) {
        pauseAfterNextBanner.classList.toggle('hidden', !active);
    }
}

function updateEventWindowDisplay(eventName, eventEndTime) {
    if (!eventWindowInfo) return;
    if (eventEndTime && eventEndTime > 0) {
        const endDate = new Date(eventEndTime * 1000);
        const endStr = endDate.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit', hour12: true });
        eventNameDisplay.textContent = eventName || 'Event';
        eventEndDisplay.textContent = `Booking ends: ${endStr}`;
        eventWindowInfo.classList.remove('hidden');
    } else {
        eventWindowInfo.classList.add('hidden');
    }
}

function updateConnectionStatus(connected) {
    const statusDot = document.getElementById('connection-status');
    const connectionText = document.querySelector('.connection-text');
    if (statusDot) {
        statusDot.className = connected ? 'status-connected' : 'status-disconnected';
        statusDot.title = connected ? 'Connected' : 'Disconnected';
    }
    if (connectionText) {
        connectionText.textContent = connected ? 'Connected' : 'Connecting...';
    }
}

// Client-side clock: tick every second using offset from server time
function parseServerTime(timeStr) {
    // Parse "hh:mm:ss am/pm" format
    const match = timeStr.match(/^(\d{1,2}):(\d{2}):(\d{2})\s*(am|pm)$/i);
    if (!match) return null;
    let h = parseInt(match[1]);
    const m = parseInt(match[2]);
    const s = parseInt(match[3]);
    const ampm = match[4].toLowerCase();
    if (ampm === 'am' && h === 12) h = 0;
    if (ampm === 'pm' && h !== 12) h += 12;
    // Create a Date with today's date and the server's time
    const now = new Date();
    return new Date(now.getFullYear(), now.getMonth(), now.getDate(), h, m, s);
}

function formatClock(date) {
    let h = date.getHours();
    const m = String(date.getMinutes()).padStart(2, '0');
    const s = String(date.getSeconds()).padStart(2, '0');
    const ampm = h >= 12 ? 'pm' : 'am';
    h = h % 12 || 12;
    return `${String(h).padStart(2, '0')}:${m}:${s} ${ampm}`;
}

setInterval(() => {
    if (serverTimeOffset && timeElement) {
        const serverNow = new Date(Date.now() + serverTimeOffset);
        timeElement.textContent = formatClock(serverNow);
    }
}, 1000);

function updateNTPStatus(data) {
    if (!ntpStatusElement) return;
    if (data.synced) {
        ntpStatusElement.className = 'ntp-status ntp-synced';
        ntpStatusElement.textContent = '\u2713';
        if (data.time && timeElement) {
            const serverDate = parseServerTime(data.time);
            if (serverDate) {
                serverTimeOffset = serverDate.getTime() - Date.now();
            }
            timeElement.textContent = data.time;
        }
        let tooltip = 'Time synced via NTP';
        if (data.timezone) {
            tooltip += `\nTimezone: ${data.timezone}`;
            const timezoneSelect = document.getElementById('timezone-select');
            if (timezoneSelect) {
                timezoneSelect.value = data.timezone;
                timezoneSelect.dataset.currentTimezone = data.timezone;
            }
        }
        ntpStatusElement.title = tooltip;
    } else {
        ntpStatusElement.className = 'ntp-status ntp-error';
        ntpStatusElement.textContent = '\u2717';
        ntpStatusElement.title = 'Time not synced';
    }
}

// --- Upcoming Events ---

function renderUpcomingEvents(events, lastSync) {
    if (!upcomingEventsList) return;
    if (!events || events.length === 0) {
        upcomingEventsList.innerHTML = '<p class="text-secondary">No upcoming events</p>';
    } else {
        let html = '';
        events.forEach(ev => {
            const startDate = new Date(ev.startTime * 1000);
            const endDate = new Date(ev.endTime * 1000);
            const dayStr = startDate.toLocaleDateString('en-US', { weekday: 'short', month: 'short', day: 'numeric' });
            const startStr = startDate.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit', hour12: true });
            const endStr = endDate.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit', hour12: true });
            const triggeredClass = ev.triggered ? ' event-triggered' : '';
            html += `
                <div class="upcoming-event-item${triggeredClass}">
                    <div class="event-name">${escapeHtml(ev.name)}</div>
                    <div class="event-time">${dayStr} ${startStr} - ${endStr}</div>
                    <div class="event-config">${ev.durationMin}min rounds${ev.numRounds ? ', ' + ev.numRounds + ' rounds' : ', continuous'}${ev.triggered ? ' (done)' : ''}</div>
                </div>
            `;
        });
        upcomingEventsList.innerHTML = html;
    }
    if (lastSyncInfo && lastSync) {
        const syncDate = new Date(lastSync);
        lastSyncInfo.textContent = `Last sync: ${syncDate.toLocaleTimeString()}`;
    }
}

// --- QR Code ---

function showQrPage() {
    sendWebSocketMessage({ action: 'get_qr_config' });
    mainPage.classList.add('hidden');
    settingsPage.classList.add('hidden');
    userManagementPage.classList.add('hidden');
    if (helpPage) helpPage.classList.add('hidden');
    if (qrPage) qrPage.classList.remove('hidden');
}

function hideQrPage() {
    if (qrPage) qrPage.classList.add('hidden');
    mainPage.classList.remove('hidden');
}

// Escape special characters in WIFI: QR strings per ZXing spec
// Characters that must be escaped: \ ; , " :
function escapeWifi(str) {
    if (!str) return '';
    return str.replace(/[\\;,":]/g, c => '\\' + c);
}

function renderQrCode(container, text) {
    if (!container || !text) return;
    container.innerHTML = '';
    try {
        // Pass container div directly — library creates canvas and appends it
        QrCreator.render({
            text,
            radius: 0.0,
            ecLevel: 'L',
            fill: '#1f2937',
            background: '#ffffff',
            size: 300
        }, container);
        // Verify canvas was actually appended
        if (!container.querySelector('canvas')) {
            throw new Error('QR library failed to generate code (text may be too long)');
        }
    } catch(e) {
        console.error('QR render error:', e, 'text:', text);
        container.innerHTML = '<p style="color:#ef4444;font-size:11px;text-align:center;padding:10px">QR Error:<br>' + e.message + '</p>';
    }
}

function renderQrCodes(config) {
    if (!config) return;

    // WiFi QR
    if (qrWifiCanvas && config.ssid) {
        const enc = config.encryption || 'WPA';
        const wifiStr = `WIFI:T:${enc};S:${escapeWifi(config.ssid)};P:${escapeWifi(config.password || '')};;`;
        renderQrCode(qrWifiCanvas, wifiStr);
        const ssidLabel = document.getElementById('qr-wifi-label');
        if (ssidLabel) ssidLabel.textContent = config.ssid;
    }

    // App URL QR
    if (qrAppCanvas && config.appUrl) {
        renderQrCode(qrAppCanvas, config.appUrl);
        const urlLabel = document.getElementById('qr-app-url');
        if (urlLabel) urlLabel.textContent = config.appUrl;
    }
}

// --- WebSocket ---

function sendWebSocketMessage(payload) {
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify(payload));
    } else {
        console.error("WebSocket is not connected.");
        showTemporaryMessage("Not connected to timer. Reconnecting...", "error");
    }
}

function clampInput(input) {
    let val = parseInt(input.value);
    const min = parseInt(input.min);
    const max = parseInt(input.max);
    if (isNaN(val)) val = min;
    if (val < min) val = min;
    if (val > max) val = max;
    input.value = val;
    return val;
}

function sendSettings() {
    clampInput(gameDurationInput);
    clampInput(numRoundsInput);
    clampInput(sirenLengthInput);
    clampInput(sirenPauseInput);
    const newSettings = {
        gameDuration: parseInt(gameDurationInput.value) * 60000,
        numRounds: parseInt(numRoundsInput.value),
        sirenLength: parseInt(sirenLengthInput.value),
        sirenPause: parseInt(sirenPauseInput.value)
    };
    sendWebSocketMessage({ action: 'save_settings', settings: newSettings });
}

function validateAndSaveAllSettings() {
    sendSettings();
    showSettingsPage(false);
}

// --- User Management ---

function loadOperators() {
    sendWebSocketMessage({ action: 'get_operators' });
}

function renderOperatorsList(operators) {
    if (!operatorsList) return;
    if (operators.length === 0) {
        operatorsList.innerHTML = '<p class="text-secondary">No operators configured</p>';
        return;
    }
    let html = '';
    operators.forEach(username => {
        const safeUsername = escapeHtml(username);
        html += `
            <div class="operator-item">
                <span class="operator-username">${safeUsername}</span>
                <button class="btn btn-danger btn-small" data-username="${safeUsername}">Remove</button>
            </div>
        `;
    });
    operatorsList.innerHTML = html;

    // Attach event listeners via delegation (avoids XSS from onclick attributes)
    operatorsList.querySelectorAll('button[data-username]').forEach(btn => {
        btn.addEventListener('click', () => {
            const username = btn.dataset.username;
            if (confirm(`Remove operator "${username}"?`)) {
                sendWebSocketMessage({ action: 'remove_operator', username: username });
            }
        });
    });
}

// --- Event Listeners ---

function initializeEventListeners() {
    settingsIcon.addEventListener('click', () => showSettingsPage(true));
    saveSettingsBtn.addEventListener('click', validateAndSaveAllSettings);

    gameDurationInput.addEventListener('change', sendSettings);
    numRoundsInput.addEventListener('change', sendSettings);
    sirenLengthInput.addEventListener('change', sendSettings);
    sirenPauseInput.addEventListener('change', sendSettings);

    // Timezone
    const timezoneSelect = document.getElementById('timezone-select');
    if (timezoneSelect) {
        timezoneSelect.addEventListener('change', () => {
            if (userRole !== 'admin') {
                showTemporaryMessage('Only administrators can change timezone', 'error');
                return;
            }
            const newTimezone = timezoneSelect.value;
            if (confirm(`Change timezone to ${newTimezone}?`)) {
                sendWebSocketMessage({ action: 'set_timezone', timezone: newTimezone });
            } else {
                updateNTPStatus({ synced: true, timezone: timezoneSelect.dataset.currentTimezone || 'Pacific/Auckland' });
            }
        });
    }

    // Auth
    if (loginBtn) {
        loginBtn.addEventListener('click', () => {
            const username = loginUsernameInput?.value.trim();
            const password = loginPasswordInput?.value.trim();
            if (!username || !password) {
                showTemporaryMessage('Please enter username and password', 'error');
                return;
            }
            sendWebSocketMessage({ action: 'authenticate', username, password });
        });
    }

    if (viewerBtn) {
        viewerBtn.addEventListener('click', () => {
            userRole = 'viewer';
            currentUsername = 'Viewer';
            loginModal.classList.add('hidden');
            updateUIForRole();
        });
    }

    if (loginDifferentBtn) {
        loginDifferentBtn.addEventListener('click', () => {
            loginModal.classList.remove('hidden');
            if (loginUsernameInput) loginUsernameInput.value = '';
            if (loginPasswordInput) loginPasswordInput.value = '';
        });
    }

    if (userIcon) {
        userIcon.addEventListener('click', () => {
            if (userRole === 'admin') {
                showUserManagementPage(true);
                loadOperators();
            } else {
                showTemporaryMessage('Admin access required', 'error');
            }
        });
    }

    if (closeUserManagementBtn) {
        closeUserManagementBtn.addEventListener('click', () => showUserManagementPage(false));
    }

    if (addOperatorBtn) {
        addOperatorBtn.addEventListener('click', () => {
            const username = newOperatorUsernameInput?.value.trim();
            const password = newOperatorPasswordInput?.value.trim();
            if (!username || !password) {
                showTemporaryMessage('Please enter username and password', 'error');
                return;
            }
            sendWebSocketMessage({ action: 'add_operator', username, password });
        });
    }

    if (changePasswordBtn) {
        changePasswordBtn.addEventListener('click', () => {
            const oldPassword = changeOldPasswordInput?.value.trim();
            const newPassword = changeNewPasswordInput?.value.trim();
            if (!oldPassword || !newPassword) {
                showTemporaryMessage('Please enter old and new passwords', 'error');
                return;
            }
            sendWebSocketMessage({ action: 'change_password', username: currentUsername, oldPassword, newPassword });
        });
    }

    if (factoryResetBtn) {
        factoryResetBtn.addEventListener('click', () => {
            if (confirm('Factory reset will restore all settings and user accounts to defaults. This cannot be undone. Continue?')) {
                sendWebSocketMessage({ action: 'factory_reset' });
            }
        });
    }

    // Help page
    if (helpIcon) {
        helpIcon.addEventListener('click', () => showHelpPage(true));
    }
    if (closeHelpBtn) {
        closeHelpBtn.addEventListener('click', () => showHelpPage(false));
    }

    // QR page
    if (qrIcon) {
        qrIcon.addEventListener('click', showQrPage);
    }
    if (qrCloseBtn) {
        qrCloseBtn.addEventListener('click', hideQrPage);
    }

    // Hello Club settings
    const saveHcSettingsBtn = document.getElementById('save-hc-settings-btn');
    if (saveHcSettingsBtn) {
        saveHcSettingsBtn.addEventListener('click', () => {
            const apiKey = document.getElementById('hc-api-key')?.value.trim();
            const enabled = document.getElementById('hc-enabled')?.checked;
            const durInput = document.getElementById('hc-default-duration');
            if (durInput) clampInput(durInput);
            const defaultDuration = durInput ? parseInt(durInput.value) : 21;
            const payload = { action: 'save_helloclub_settings', enabled, defaultDuration };
            if (apiKey && apiKey !== '***configured***') payload.apiKey = apiKey;
            sendWebSocketMessage(payload);
        });
    }

    // QR settings save
    const saveQrSettingsBtn = document.getElementById('save-qr-settings-btn');
    if (saveQrSettingsBtn) {
        saveQrSettingsBtn.addEventListener('click', () => {
            const ssid = document.getElementById('qr-wifi-ssid')?.value.trim() || '';
            const password = document.getElementById('qr-wifi-password')?.value || '';
            const encryption = document.getElementById('qr-wifi-encryption')?.value || 'WPA';
            sendWebSocketMessage({ action: 'save_qr_settings', ssid, password, encryption });
        });
    }

    // Sync now
    if (syncNowBtn) {
        syncNowBtn.addEventListener('click', () => {
            showTemporaryMessage('Syncing with Hello Club...', 'success');
            sendWebSocketMessage({ action: 'helloclub_refresh' });
        });
    }

    // Keyboard support for login
    if (loginPasswordInput) {
        loginPasswordInput.addEventListener('keypress', (e) => {
            if (e.key === 'Enter') loginBtn?.click();
        });
    }
    if (loginUsernameInput) {
        loginUsernameInput.addEventListener('keypress', (e) => {
            if (e.key === 'Enter') loginBtn?.click();
        });
    }

    // Timer controls
    startBtn.addEventListener('click', () => {
        playBeep(880, 100);
        disableButtonsTemporarily();
        sendWebSocketMessage({ action: 'start' });
    });

    pauseBtn.addEventListener('click', () => {
        playBeep(660, 100);
        disableButtonsTemporarily();
        sendWebSocketMessage({ action: 'pause' });
    });

    if (pauseAfterNextBtn) {
        pauseAfterNextBtn.addEventListener('click', () => {
            playBeep(550, 100);
            const newState = !pauseAfterNextActive;
            sendWebSocketMessage({ action: 'pause_after_next', enabled: newState });
        });
    }

    resetBtn.addEventListener('click', () => {
        playBeep(440, 150);
        disableButtonsTemporarily();
        sendWebSocketMessage({ action: 'reset' });
    });

    // Keyboard shortcuts
    document.addEventListener('keydown', (e) => {
        if (e.target.matches('input, select')) return;
        switch (e.key.toLowerCase()) {
            case ' ':
                e.preventDefault();
                pauseBtn.click();
                break;
            case 'r':
                e.preventDefault();
                resetBtn.click();
                break;
            case 's':
                e.preventDefault();
                if (!startBtn.disabled) startBtn.click();
                break;
            case 'm':
                e.preventDefault();
                soundEnabled = !soundEnabled;
                showTemporaryMessage(`Sound ${soundEnabled ? 'enabled' : 'disabled'}`, 'success');
                break;
            case 'escape':
                hideQrPage();
                if (helpPage && !helpPage.classList.contains('hidden')) showHelpPage(false);
                break;
            case '?':
                e.preventDefault();
                showTemporaryMessage('Keys: SPACE=Pause, R=Reset, S=Start, M=Sound, ESC=Close', 'success');
                break;
        }
    });
}

// --- WebSocket Connection ---

function connectWebSocket() {
    if (reconnectTimeout) {
        clearTimeout(reconnectTimeout);
        reconnectTimeout = null;
    }

    console.log('Connecting to WebSocket...');
    showLoadingOverlay('Connecting to timer...');
    socket = new WebSocket(`ws://${window.location.host}/ws`);

    socket.onopen = () => {
        console.log('WebSocket connected');
        hideLoadingOverlay();
        reconnectAttempts = 0;
        updateConnectionStatus(true);
        if (loginModal) loginModal.classList.remove('hidden');
        // Request upcoming events
        sendWebSocketMessage({ action: 'get_upcoming_events' });
    };

    socket.onclose = () => {
        console.log('WebSocket disconnected');
        updateConnectionStatus(false);
        stopClientTimer();
        reconnectAttempts++;
        if (reconnectAttempts > MAX_RECONNECT_ATTEMPTS) {
            showTemporaryMessage("Cannot connect to timer. Please refresh the page.", "error");
            return;
        }
        const delay = Math.min(1000 * Math.pow(2, reconnectAttempts), 30000);
        console.log(`Reconnecting in ${delay / 1000}s (attempt ${reconnectAttempts})...`);
        showTemporaryMessage(`Reconnecting in ${Math.ceil(delay / 1000)}s...`, "error");
        reconnectTimeout = setTimeout(connectWebSocket, delay);
    };

    socket.onerror = (error) => {
        console.error('WebSocket error:', error);
    };

    socket.onmessage = (event) => {
        const data = JSON.parse(event.data);

        switch (data.event) {
            case 'auth_required':
                loginModal.classList.remove('hidden');
                break;

            case 'auth_success':
            case 'viewer_mode':
                userRole = data.role || 'viewer';
                currentUsername = data.username || 'Viewer';
                loginModal.classList.add('hidden');
                updateUIForRole();
                if (data.event === 'auth_success') {
                    showTemporaryMessage(`Welcome, ${escapeHtml(currentUsername)}!`, "success");
                }
                break;

            case 'login_prompt':
                // Server greeting on connect — show login modal
                loginModal.classList.remove('hidden');
                break;

            case 'session_timeout':
                userRole = 'viewer';
                currentUsername = 'Viewer';
                updateUIForRole();
                loginModal.classList.remove('hidden');
                showTemporaryMessage(data.message || "Session expired", "error");
                break;

            case 'auth_failed':
                showTemporaryMessage(data.message || "Authentication failed", "error");
                loginModal.classList.remove('hidden');
                break;

            case 'pause_after_next_changed':
                if (typeof data.pauseAfterNext !== 'undefined') {
                    updatePauseAfterNextUI(data.pauseAfterNext);
                }
                break;

            case 'operators_list':
                renderOperatorsList(data.operators || []);
                break;

            case 'operator_added':
                showTemporaryMessage(`Operator "${escapeHtml(data.username)}" added`, "success");
                if (newOperatorUsernameInput) newOperatorUsernameInput.value = '';
                if (newOperatorPasswordInput) newOperatorPasswordInput.value = '';
                loadOperators();
                break;

            case 'operator_removed':
                showTemporaryMessage(`Operator "${escapeHtml(data.username)}" removed`, "success");
                loadOperators();
                break;

            case 'password_changed':
                showTemporaryMessage("Password changed successfully", "success");
                if (changeOldPasswordInput) changeOldPasswordInput.value = '';
                if (changeNewPasswordInput) changeNewPasswordInput.value = '';
                break;

            case 'timezone_changed':
                showTemporaryMessage(`Timezone updated to ${data.timezone}`, "success");
                const tzSelect = document.getElementById('timezone-select');
                if (tzSelect) tzSelect.dataset.currentTimezone = data.timezone;
                break;

            case 'factory_reset_complete':
                showTemporaryMessage("Factory reset complete. Please refresh page.", "success");
                setTimeout(() => window.location.reload(), 2000);
                break;

            case 'ntp_status':
                updateNTPStatus(data);
                break;

            case 'upcoming_events':
                renderUpcomingEvents(data.events || [], data.lastSync);
                break;

            case 'helloclub_settings':
                const hcApiKeyInput = document.getElementById('hc-api-key');
                const hcEnabledInput = document.getElementById('hc-enabled');
                const hcStatusEl = document.getElementById('hc-api-status');
                if (hcApiKeyInput) {
                    hcApiKeyInput.placeholder = data.apiKey === '***configured***'
                        ? 'API key is set (leave blank to keep)'
                        : 'Paste API key here';
                    hcApiKeyInput.value = '';
                }
                if (hcEnabledInput) hcEnabledInput.checked = data.enabled || false;
                const hcDefaultDurInput = document.getElementById('hc-default-duration');
                if (hcDefaultDurInput && data.defaultDuration) hcDefaultDurInput.value = data.defaultDuration;
                if (hcStatusEl) {
                    hcStatusEl.textContent = data.apiKey === '***configured***'
                        ? 'API key: configured'
                        : 'API key: not set';
                }
                break;

            case 'helloclub_settings_saved':
                showTemporaryMessage(data.message || 'Hello Club settings saved', 'success');
                // Refresh to show updated status
                sendWebSocketMessage({ action: 'get_helloclub_settings' });
                break;

            case 'helloclub_refresh_result':
                if (data.success) {
                    showTemporaryMessage(data.message || `Synced ${data.eventCount || 0} events from Hello Club`, "success");
                    if (data.debug) console.log("HC Sync Debug:\n" + data.debug);
                } else {
                    showTemporaryMessage(data.message || data.error || 'Hello Club sync failed', "error");
                    if (data.debug) console.log("HC Sync Debug:\n" + data.debug);
                }
                break;

            case 'event_auto_started':
                showTemporaryMessage(`Auto-started: ${escapeHtml(data.eventName)}`, "success");
                break;

            case 'event_cutoff':
                showTemporaryMessage(data.message || "Session ended - booking time expired", "error");
                break;

            case 'qr_config': {
                renderQrCodes(data);
                // Populate settings form
                const qrSsidInput = document.getElementById('qr-wifi-ssid');
                const qrPassInput = document.getElementById('qr-wifi-password');
                const qrEncInput = document.getElementById('qr-wifi-encryption');
                const qrSsidHint = document.getElementById('qr-connected-ssid-hint');
                if (qrSsidInput) qrSsidInput.value = data.ssidOverride || '';
                if (qrPassInput) qrPassInput.value = data.password || '';
                if (qrEncInput && data.encryption) qrEncInput.value = data.encryption;
                if (qrSsidHint && data.connectedSsid) {
                    qrSsidHint.textContent = `Connected network: ${data.connectedSsid}`;
                }
                break;
            }

            case 'qr_settings_saved':
                showTemporaryMessage('QR settings saved', 'success');
                break;

            case 'error':
                hideLoadingOverlay();
                showTemporaryMessage(data.message || "An error occurred", "error");
                break;

            // --- Timer events ---
            case 'start':
            case 'new_round':
                serverEndTime = Date.now() + data.gameDuration;
                displayEndTime = serverEndTime; // Snap on fresh start
                isClientTimerPaused = false;
                currentTimerStatus = 'RUNNING';
                updatePauseBtnLabel(false);
                startClientTimer();
                if (data.continuousMode !== undefined) continuousMode = data.continuousMode;
                roundCounterElement.textContent = formatRoundCounter(data.currentRound, data.numRounds);
                enableDisplay.className = 'status-active';
                updatePauseAfterNextVisibility();
                if (data.pauseAfterNext !== undefined) updatePauseAfterNextUI(data.pauseAfterNext);
                if (data.activeEventEndTime !== undefined) updateEventWindowDisplay(data.activeEventName, data.activeEventEndTime);
                break;

            case 'sync': {
                // Update the target — the animation loop will smoothly converge
                serverEndTime = Date.now() + data.mainTimerRemaining;

                isClientTimerPaused = (data.status === 'PAUSED');
                currentTimerStatus = data.status;
                updatePauseBtnLabel(isClientTimerPaused);

                if (!isClientTimerPaused && data.status === 'RUNNING') {
                    startClientTimer();
                } else {
                    stopClientTimer();
                    displayEndTime = serverEndTime;
                    mainTimerDisplay.textContent = formatTime(data.mainTimerRemaining);
                }

                if (data.continuousMode !== undefined) continuousMode = data.continuousMode;
                roundCounterElement.textContent = formatRoundCounter(data.currentRound, data.numRounds);
                enableDisplay.className = (data.status === 'RUNNING' || data.status === 'PAUSED') ? 'status-active' : 'status-idle';
                updatePauseAfterNextVisibility();
                if (data.pauseAfterNext !== undefined) updatePauseAfterNextUI(data.pauseAfterNext);
                if (data.activeEventEndTime !== undefined) updateEventWindowDisplay(data.activeEventName, data.activeEventEndTime);
                break;
            }

            case 'pause':
                isClientTimerPaused = true;
                currentTimerStatus = 'PAUSED';
                stopClientTimer();
                if (data.mainTimerRemaining !== undefined) {
                    serverEndTime = Date.now() + data.mainTimerRemaining;
                    displayEndTime = serverEndTime;
                    mainTimerDisplay.textContent = formatTime(data.mainTimerRemaining);
                } else {
                    mainTimerDisplay.textContent = formatTime(Math.max(0, displayEndTime - Date.now()));
                }
                if (data.currentRound !== undefined) {
                    roundCounterElement.textContent = formatRoundCounter(data.currentRound, data.numRounds);
                }
                updatePauseBtnLabel(true);
                updatePauseAfterNextUI(false);
                updatePauseAfterNextVisibility();
                break;

            case 'resume':
                isClientTimerPaused = false;
                currentTimerStatus = 'RUNNING';
                if (data.mainTimerRemaining !== undefined) {
                    serverEndTime = Date.now() + data.mainTimerRemaining;
                    displayEndTime = serverEndTime;
                }
                updatePauseBtnLabel(false);
                startClientTimer();
                updatePauseAfterNextVisibility();
                break;

            case 'reset':
            case 'finished':
                stopClientTimer();
                serverEndTime = 0;
                displayEndTime = 0;
                currentTimerStatus = 'IDLE';
                continuousMode = false;
                updatePauseBtnLabel(false);
                mainTimerDisplay.textContent = formatTime(0);
                enableDisplay.className = 'status-idle';
                updatePauseAfterNextUI(false);
                updatePauseAfterNextVisibility();
                updateEventWindowDisplay(null, 0);
                break;

            case 'settings':
                settings = data.settings;
                gameDurationInput.value = settings.gameDuration / 60000;
                numRoundsInput.value = settings.numRounds;
                sirenLengthInput.value = settings.sirenLength;
                sirenPauseInput.value = settings.sirenPause;
                break;

            case 'state':
                serverEndTime = Date.now() + (data.state.mainTimer || 0);
                displayEndTime = serverEndTime;
                if (data.state.status !== 'RUNNING') {
                    mainTimerDisplay.textContent = formatTime(data.state.mainTimer);
                }
                currentTimerStatus = data.state.status || 'IDLE';
                if (data.state.continuousMode !== undefined) continuousMode = data.state.continuousMode;
                roundCounterElement.textContent = formatRoundCounter(data.state.currentRound || 1, data.state.numRounds || 3);
                enableDisplay.className = (data.state.status === 'RUNNING' || data.state.status === 'PAUSED') ? 'status-active' : 'status-idle';
                if (data.state.time) {
                    const serverDate = parseServerTime(data.state.time);
                    if (serverDate) serverTimeOffset = serverDate.getTime() - Date.now();
                }
                updatePauseAfterNextVisibility();
                if (data.state.pauseAfterNext !== undefined) updatePauseAfterNextUI(data.state.pauseAfterNext);
                if (data.state.activeEventEndTime !== undefined) updateEventWindowDisplay(data.state.activeEventName, data.state.activeEventEndTime);
                break;
        }
    };
}

// --- Global handlers for onclick ---
window.showSettingsPage = showSettingsPage;
window.showUserManagementPage = showUserManagementPage;
window.showHelpPage = showHelpPage;

// --- Init ---
if (!SIMULATION_MODE) {
    initializeEventListeners();
    connectWebSocket();
} else {
    console.log("Running in Simulation Mode");
    initializeEventListeners();
}
