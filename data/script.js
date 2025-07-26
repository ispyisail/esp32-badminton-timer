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

// --- Client-side Timer State ---
let serverState = { mainTimer: 0, breakTimer: 0, status: 'IDLE' };
let lastSyncTime = 0;

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

function animationLoop() {
    if (serverState.status === 'RUNNING') {
        const elapsedSinceSync = Date.now() - lastSyncTime;
        const mainTimerNow = serverState.mainTimer - elapsedSinceSync;
        const breakTimerNow = serverState.breakTimer - elapsedSinceSync;

        mainTimerDisplay.textContent = formatTime(mainTimerNow);
        if (settings.breakTimerEnabled) {
            breakTimerDisplay.textContent = formatTime(breakTimerNow);
        }
    }
    // Continue the loop
    requestAnimationFrame(animationLoop);
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

function initializeEventListeners() {
    settingsIcon.addEventListener('click', () => showSettingsPage(true));
    saveSettingsBtn.addEventListener('click', validateAndSaveAllSettings);
    
    gameDurationInput.addEventListener('change', sendSettings);
    numRoundsInput.addEventListener('change', sendSettings);
    breakTimeInput.addEventListener('change', sendSettings);
    breakTimerEnableInput.addEventListener('change', sendSettings);
    sirenLengthInput.addEventListener('change', sendSettings);
    sirenPauseInput.addEventListener('change', sendSettings);

    startBtn.addEventListener('click', () => sendWebSocketMessage({ action: 'start' }));
    pauseBtn.addEventListener('click', () => sendWebSocketMessage({ action: 'pause' }));
    resetBtn.addEventListener('click', () => sendWebSocketMessage({ action: 'reset' }));
}

function connectWebSocket() {
    socket = new WebSocket(`ws://${window.location.hostname}/ws`);

    socket.onopen = () => {
        console.log('WebSocket connected');
        initializeEventListeners();
    };
    
    socket.onclose = () => {
        console.log('WebSocket disconnected. Reconnecting...');
        setTimeout(connectWebSocket, 2000);
    };

    socket.onmessage = (event) => {
        const data = JSON.parse(event.data);
        
        if (data.settings) {
            settings = data.settings;
            gameDurationInput.value = settings.gameDuration;
            breakTimeInput.value = settings.breakDuration;
            numRoundsInput.value = settings.numRounds;
            breakTimerEnableInput.checked = settings.breakTimerEnabled;
            sirenLengthInput.value = settings.sirenLength;
            sirenPauseInput.value = settings.sirenPause;
        }

        serverState = data;
        lastSyncTime = Date.now();
        updateStaticUI(data);

        // If the timer is not running, make sure the display shows the final, authoritative time
        if (serverState.status !== 'RUNNING') {
            mainTimerDisplay.textContent = formatTime(serverState.mainTimer);
            breakTimerDisplay.textContent = formatTime(serverState.breakTimer);
        }
    };
}

// --- Main Execution ---
if (!SIMULATION_MODE) {
    connectWebSocket();
    requestAnimationFrame(animationLoop); // Start the animation loop
} else {
    console.log("Running in Simulation Mode");
    initializeEventListeners();
}