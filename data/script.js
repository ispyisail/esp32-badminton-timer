// --- Get elements ---
const mainPage = document.getElementById('main-page');
const settingsPage = document.getElementById('settings-page');
const enableDisplay = document.getElementById('enable-display');
const timeElement = document.getElementById('time');
const roundCounterElement = document.getElementById('round-counter');
const mainTimerDisplay = document.getElementById('main-timer-display');
const breakTimerDisplay = document.getElementById('break-timer-display');
const startBtn = document.getElementById('start-btn');
const pauseBtn = document.getElementById('pause-btn');
const resetBtn = document.getElementById('reset-btn');
const settingsIcon = document.getElementById('settings-icon');
const saveSettingsBtn = document.getElementById('save-settings-btn');

// --- Settings inputs ---
const gameDurationInput = document.getElementById('game-duration');
const breakTimeInput = document.getElementById('break-time');
const numRoundsInput = document.getElementById('num-rounds');
const breakTimerEnableInput = document.getElementById('break-timer-enable');
const sirenLengthInput = document.getElementById('siren-length');
const sirenPauseInput = document.getElementById('siren-pause');

// --- State and Settings ---
const SIMULATION_MODE = window.location.protocol.startsWith('file');
let socket;
let settings = {};

// --- Client-side Timer State (new requestAnimationFrame model) ---
let serverMainTimerRemaining = 0;
let serverBreakTimerRemaining = 0;
let lastSyncTime = 0; // Local timestamp when last sync received
let isClientTimerPaused = false;
let animationFrameId = null;

// --- Reconnection with exponential backoff ---
let reconnectAttempts = 0;
const MAX_RECONNECT_ATTEMPTS = 10;
let reconnectTimeout = null;

// --- Authentication state ---
let isAuthenticated = false;

// --- Button state tracking ---
let buttonsEnabled = true;

// --- Sound effects configuration ---
let soundEnabled = true; // Can be toggled by user
const audioContext = new (window.AudioContext || window.webkitAudioContext)();

// --- Function Definitions ---

/**
 * @brief Play a beep sound using Web Audio API
 * @param frequency Frequency in Hz (e.g., 440 for A4 note)
 * @param duration Duration in milliseconds
 */
function playBeep(frequency = 440, duration = 100) {
    if (!soundEnabled) return;

    const oscillator = audioContext.createOscillator();
    const gainNode = audioContext.createGain();

    oscillator.connect(gainNode);
    gainNode.connect(audioContext.destination);

    oscillator.frequency.value = frequency;
    gainNode.gain.setValueAtTime(0.3, audioContext.currentTime);
    gainNode.gain.exponentialRampToValueAtTime(0.01, audioContext.currentTime + duration/1000);

    oscillator.start();
    oscillator.stop(audioContext.currentTime + duration/1000);
}

/**
 * @brief Show loading overlay
 * @param message Message to display
 */
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

/**
 * @brief Hide loading overlay
 */
function hideLoadingOverlay() {
    const overlay = document.getElementById('loading-overlay');
    if (overlay) {
        overlay.style.display = 'none';
    }
}

/**
 * @brief Disable buttons temporarily
 * @param duration Duration in milliseconds
 */
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

// --- Function Definitions ---

function formatTime(milliseconds) {
    if (isNaN(milliseconds) || milliseconds < 0) milliseconds = 0;
    const totalSeconds = Math.floor(milliseconds / 1000);
    const minutes = Math.floor(totalSeconds / 60);
    const seconds = totalSeconds % 60;
    return `${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`;
}

function updateStaticUI(state) {
    timeElement.textContent = state.time || '--:--:--';
    roundCounterElement.textContent = `Round ${state.currentRound || 1} of ${state.numRounds || 3}`;
    enableDisplay.className = (state.status === 'RUNNING' || state.status === 'PAUSED') ? 'status-active' : 'status-idle';
    breakTimerDisplay.style.display = settings.breakTimerEnabled ? 'block' : 'none';
}

// New requestAnimationFrame-based timer loop for smooth, drift-free countdown
function clientTimerLoop() {
    if (isClientTimerPaused) {
        animationFrameId = requestAnimationFrame(clientTimerLoop);
        return;
    }

    // Calculate elapsed time since last sync
    const localElapsed = Date.now() - lastSyncTime;

    // Calculate current timer values based on server sync + local elapsed
    let currentMainTimer = serverMainTimerRemaining - localElapsed;
    let currentBreakTimer = serverBreakTimerRemaining - localElapsed;

    // Prevent negative values
    if (currentMainTimer < 0) currentMainTimer = 0;
    if (currentBreakTimer < 0) currentBreakTimer = 0;

    // Update display
    mainTimerDisplay.textContent = formatTime(currentMainTimer);
    breakTimerDisplay.textContent = formatTime(currentBreakTimer);

    // Continue animation loop if timer still running
    if (currentMainTimer > 0 || currentBreakTimer > 0) {
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

function showSettingsPage(show) {
    mainPage.classList.toggle('hidden', show);
    settingsPage.classList.toggle('hidden', !show);
}

function sendWebSocketMessage(payload) {
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify(payload));
    } else {
        console.error("WebSocket is not connected.");
        showTemporaryMessage("Not connected to timer. Reconnecting...", "error");
    }
}

function sendSettings() {
    const newSettings = {
        gameDuration: parseInt(gameDurationInput.value),
        breakDuration: parseInt(breakTimeInput.value),
        numRounds: parseInt(numRoundsInput.value),
        breakTimerEnabled: breakTimerEnableInput.checked,
        sirenLength: parseInt(sirenLengthInput.value),
        sirenPause: parseInt(sirenPauseInput.value)
    };
    sendWebSocketMessage({ action: 'save_settings', settings: newSettings });
}

function validateAndSaveAllSettings() {
    let gameDuration = parseInt(gameDurationInput.value);
    let breakDuration = parseInt(breakTimeInput.value);
    let numRounds = parseInt(numRoundsInput.value);

    if (gameDuration < 1) gameDuration = 1;
    if (gameDuration > 60) gameDuration = 60;
    if (numRounds < 1) numRounds = 1;
    if (numRounds > 20) numRounds = 20;
    if (breakDuration < 1) breakDuration = 1;

    const gameDurationInSeconds = gameDuration * 60;
    if (breakDuration > gameDurationInSeconds * 0.5) {
        breakDuration = Math.floor(gameDurationInSeconds * 0.5);
    }

    gameDurationInput.value = gameDuration;
    numRoundsInput.value = numRounds;
    breakTimeInput.value = breakDuration;

    sendSettings();
    showSettingsPage(false);
}

function showTemporaryMessage(msg, type = "success") {
    const banner = document.createElement('div');
    banner.textContent = msg;
    banner.className = `temp-message temp-message-${type}`;
    banner.style.cssText = `position:fixed;top:20px;left:50%;transform:translateX(-50%);
        background:${type === 'error' ? '#dc3545' : '#28a745'};color:white;padding:12px 24px;
        border-radius:6px;z-index:10000;box-shadow:0 4px 8px rgba(0,0,0,0.3);
        font-weight:bold;animation:fadeIn 0.3s ease-in;`;
    document.body.appendChild(banner);
    setTimeout(() => {
        banner.style.animation = 'fadeOut 0.3s ease-out';
        setTimeout(() => banner.remove(), 300);
    }, 3000);
}

function promptPassword() {
    const password = prompt("Please enter the timer control password:");
    if (password) {
        sendWebSocketMessage({ action: 'authenticate', password: password });
    } else {
        showTemporaryMessage("Password required to control timer", "error");
        setTimeout(promptPassword, 2000); // Ask again
    }
}

function updateConnectionStatus(connected) {
    const statusDot = document.getElementById('connection-status');
    if (statusDot) {
        statusDot.className = connected ? 'status-connected' : 'status-disconnected';
        statusDot.title = connected ? 'Connected' : 'Disconnected';
    }
}

function initializeEventListeners() {
    settingsIcon.addEventListener('click', () => showSettingsPage(true));
    saveSettingsBtn.addEventListener('click', validateAndSaveAllSettings);

    gameDurationInput.addEventListener('change', sendSettings);
    numRoundsInput.addEventListener('change', sendSettings);
    breakTimeInput.addEventListener('change', sendSettings);
    breakTimerEnableInput.addEventListener('change', sendSettings);
    sirenLengthInput.addEventListener('change', sendSettings);
    sirenPauseInput.addEventListener('change', sendSettings);

    // Button click handlers with sound and disabled state
    startBtn.addEventListener('click', () => {
        playBeep(880, 100); // High beep for start
        disableButtonsTemporarily();
        sendWebSocketMessage({ action: 'start' });
    });

    pauseBtn.addEventListener('click', () => {
        playBeep(660, 100); // Medium beep for pause/resume
        disableButtonsTemporarily();
        sendWebSocketMessage({ action: 'pause' });
    });

    resetBtn.addEventListener('click', () => {
        playBeep(440, 150); // Low beep for reset
        disableButtonsTemporarily();
        sendWebSocketMessage({ action: 'reset' });
    });

    // Keyboard shortcuts
    document.addEventListener('keydown', (e) => {
        // Don't trigger shortcuts when typing in input fields
        if (e.target.matches('input')) return;

        switch(e.key.toLowerCase()) {
            case ' ': // Spacebar for pause/resume
                e.preventDefault();
                pauseBtn.click();
                break;
            case 'r': // R for reset
                e.preventDefault();
                resetBtn.click();
                break;
            case 's': // S for start
                e.preventDefault();
                if (startBtn.disabled === false) {
                    startBtn.click();
                }
                break;
            case 'm': // M to toggle sound
                e.preventDefault();
                soundEnabled = !soundEnabled;
                showTemporaryMessage(`Sound ${soundEnabled ? 'enabled' : 'disabled'}`, 'success');
                break;
            case '?': // ? for help
                e.preventDefault();
                showTemporaryMessage('Keyboard: SPACE=Pause/Resume, R=Reset, S=Start, M=Toggle Sound', 'success');
                break;
        }
    });
}

function connectWebSocket() {
    // Clear any existing reconnect timeout
    if (reconnectTimeout) {
        clearTimeout(reconnectTimeout);
        reconnectTimeout = null;
    }

    console.log('Connecting to WebSocket...');
    showLoadingOverlay('Connecting to timer...');
    socket = new WebSocket(`ws://${window.location.hostname}/ws`);

    socket.onopen = () => {
        console.log('WebSocket connected');
        hideLoadingOverlay();
        reconnectAttempts = 0; // Reset on successful connection
        updateConnectionStatus(true);
        isAuthenticated = false; // Will need to authenticate
    };

    socket.onclose = () => {
        console.log('WebSocket disconnected');
        updateConnectionStatus(false);
        stopClientTimer();
        isAuthenticated = false;

        // Exponential backoff reconnection
        reconnectAttempts++;

        if (reconnectAttempts > MAX_RECONNECT_ATTEMPTS) {
            showTemporaryMessage("Cannot connect to timer. Please refresh the page.", "error");
            return;
        }

        const delay = Math.min(1000 * Math.pow(2, reconnectAttempts), 30000);
        console.log(`Reconnecting in ${delay/1000}s (attempt ${reconnectAttempts})...`);
        showTemporaryMessage(`Reconnecting in ${Math.ceil(delay/1000)}s...`, "error");

        reconnectTimeout = setTimeout(connectWebSocket, delay);
    };

    socket.onerror = (error) => {
        console.error('WebSocket error:', error);
    };

    socket.onmessage = (event) => {
        const data = JSON.parse(event.data);

        switch (data.event) {
            case 'auth_required':
                promptPassword();
                break;

            case 'auth_success':
                isAuthenticated = true;
                showTemporaryMessage("Authentication successful!", "success");
                break;

            case 'error':
                showTemporaryMessage(data.message || "An error occurred", "error");
                break;

            case 'start':
            case 'new_round':
                serverMainTimerRemaining = data.gameDuration;
                serverBreakTimerRemaining = data.breakDuration;
                lastSyncTime = Date.now();
                isClientTimerPaused = false;
                startClientTimer();
                roundCounterElement.textContent = `Round ${data.currentRound} of ${data.numRounds}`;
                enableDisplay.className = 'status-active';
                break;

            case 'sync':
                // Server sync with actual remaining time
                serverMainTimerRemaining = data.mainTimerRemaining;
                serverBreakTimerRemaining = data.breakTimerRemaining;
                lastSyncTime = Date.now(); // Reset sync timestamp
                isClientTimerPaused = (data.status === 'PAUSED');

                if (!isClientTimerPaused) {
                    startClientTimer();
                } else {
                    stopClientTimer();
                }

                roundCounterElement.textContent = `Round ${data.currentRound} of ${data.numRounds}`;
                enableDisplay.className = (data.status === 'RUNNING' || data.status === 'PAUSED') ? 'status-active' : 'status-idle';
                break;

            case 'pause':
                isClientTimerPaused = true;
                stopClientTimer();
                // Update displays to current values
                mainTimerDisplay.textContent = formatTime(serverMainTimerRemaining - (Date.now() - lastSyncTime));
                breakTimerDisplay.textContent = formatTime(serverBreakTimerRemaining - (Date.now() - lastSyncTime));
                break;

            case 'resume':
                isClientTimerPaused = false;
                startClientTimer();
                break;

            case 'reset':
            case 'finished':
                stopClientTimer();
                serverMainTimerRemaining = 0;
                serverBreakTimerRemaining = 0;
                mainTimerDisplay.textContent = formatTime(0);
                breakTimerDisplay.textContent = formatTime(0);
                enableDisplay.className = 'status-idle';
                break;

            case 'settings':
                settings = data.settings;
                gameDurationInput.value = settings.gameDuration / 60000;
                breakTimeInput.value = settings.breakDuration / 1000;
                numRoundsInput.value = settings.numRounds;
                breakTimerEnableInput.checked = settings.breakTimerEnabled;
                sirenLengthInput.value = settings.sirenLength;
                sirenPauseInput.value = settings.sirenPause;
                breakTimerDisplay.style.display = settings.breakTimerEnabled ? 'block' : 'none';
                break;

            case 'state':
                updateStaticUI(data.state);
                if (data.state.status !== 'RUNNING') {
                    mainTimerDisplay.textContent = formatTime(data.state.mainTimer);
                    breakTimerDisplay.textContent = formatTime(data.state.breakTimer);
                }
                break;
        }
    };
}

// --- Main Execution ---
if (!SIMULATION_MODE) {
    initializeEventListeners();
    connectWebSocket();
} else {
    console.log("Running in Simulation Mode");
    initializeEventListeners();
}
