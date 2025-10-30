// --- Get elements ---
const mainPage = document.getElementById('main-page');
const settingsPage = document.getElementById('settings-page');
const userManagementPage = document.getElementById('user-management-page');
const schedulePage = document.getElementById('schedule-page');
const loginModal = document.getElementById('login-modal');
const enableDisplay = document.getElementById('enable-display');
const timeElement = document.getElementById('time');
const ntpStatusElement = document.getElementById('ntp-status');
const roundCounterElement = document.getElementById('round-counter');
const mainTimerDisplay = document.getElementById('main-timer-display');
const breakTimerDisplay = document.getElementById('break-timer-display');
const startBtn = document.getElementById('start-btn');
const pauseBtn = document.getElementById('pause-btn');
const resetBtn = document.getElementById('reset-btn');
const settingsIcon = document.getElementById('settings-icon');
const scheduleIcon = document.getElementById('schedule-icon');
const userIcon = document.getElementById('user-icon');
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

// --- Schedule elements ---
const schedulingEnabledToggle = document.getElementById('scheduling-enabled-toggle');
const scheduleClubFilter = document.getElementById('schedule-club-filter');
const loadSchedulesBtn = document.getElementById('load-schedules-btn');
const calendarBody = document.getElementById('calendar-body');
const scheduleClubName = document.getElementById('schedule-club-name');
const scheduleDay = document.getElementById('schedule-day');
const scheduleHour = document.getElementById('schedule-hour');
const scheduleMinute = document.getElementById('schedule-minute');
const scheduleDuration = document.getElementById('schedule-duration');
const scheduleEnabled = document.getElementById('schedule-enabled');
const saveScheduleBtn = document.getElementById('save-schedule-btn');
const cancelScheduleBtn = document.getElementById('cancel-schedule-btn');
const schedulesList = document.getElementById('schedules-list');
const closeScheduleBtn = document.getElementById('close-schedule-btn');

// --- Hello Club elements ---
const helloClubPage = document.getElementById('helloclub-page');
const helloClubIcon = document.getElementById('helloclub-icon');
const helloClubEnabled = document.getElementById('helloclub-enabled');
const helloClubApiKey = document.getElementById('helloclub-api-key');
const helloClubDaysAhead = document.getElementById('helloclub-days-ahead');
const helloClubSyncHour = document.getElementById('helloclub-sync-hour');
const saveHelloClubSettingsBtn = document.getElementById('save-helloclub-settings-btn');
const fetchCategoriesBtn = document.getElementById('fetch-categories-btn');
const helloClubCategoriesList = document.getElementById('helloclub-categories-list');
const previewEventsBtn = document.getElementById('preview-events-btn');
const syncNowBtn = document.getElementById('sync-now-btn');
const lastSyncText = document.getElementById('last-sync-text');
const closeHelloClubBtn = document.getElementById('close-helloclub-btn');
const helloClubImportModal = document.getElementById('helloclub-import-modal');
const helloClubEventsContainer = document.getElementById('helloclub-events-container');
const importSelectedBtn = document.getElementById('import-selected-btn');
const cancelImportBtn = document.getElementById('cancel-import-btn');

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
let userRole = 'viewer'; // 'viewer', 'operator', 'admin'
let currentUsername = 'Viewer';

// --- Schedule state ---
let schedules = [];
let editingScheduleId = null;
let schedulingEnabled = false;

// --- Hello Club state ---
let helloClubCategories = [];
let helloClubEvents = [];
let selectedCategories = [];

// --- Button state tracking ---
let buttonsEnabled = true;

// --- Sound effects configuration ---
let soundEnabled = true; // Can be toggled by user
const audioContext = new (window.AudioContext || window.webkitAudioContext)();

// --- Function Definitions ---

/**
 * @brief Escape HTML to prevent XSS attacks
 * @param text Unsafe text that may contain HTML
 * @return HTML-escaped safe string
 */
function escapeHtml(text) {
    if (!text) return '';
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

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
    userManagementPage.classList.add('hidden');
    schedulePage.classList.add('hidden');
}

function showUserManagementPage(show) {
    mainPage.classList.toggle('hidden', show);
    settingsPage.classList.add('hidden');
    userManagementPage.classList.toggle('hidden', !show);
    schedulePage.classList.add('hidden');
}

function showSchedulePage(show) {
    mainPage.classList.toggle('hidden', show);
    settingsPage.classList.add('hidden');
    userManagementPage.classList.add('hidden');
    schedulePage.classList.toggle('hidden', !show);

    if (show) {
        loadSchedules();
    }
}

function updateUIForRole() {
    // Update body class for CSS-based role visibility
    document.body.className = `role-${userRole}`;

    // Update current user info
    if (currentUserInfo) {
        let roleDisplay = userRole.charAt(0).toUpperCase() + userRole.slice(1);
        let badgeClass = userRole === 'admin' ? 'badge-danger' :
                        userRole === 'operator' ? 'badge-warning' : 'badge-secondary';
        currentUserInfo.innerHTML = `
            <strong>${currentUsername}</strong>
            <span class="badge ${badgeClass}">${roleDisplay}</span>
        `;
    }
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

// --- User Management Functions ---

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
                <button class="btn btn-danger btn-small" onclick="removeOperator('${safeUsername}')">Remove</button>
            </div>
        `;
    });

    operatorsList.innerHTML = html;
}

function removeOperator(username) {
    if (confirm(`Remove operator "${username}"?`)) {
        sendWebSocketMessage({
            action: 'remove_operator',
            username: username
        });
    }
}

// --- Schedule Management Functions ---

function loadSchedules() {
    sendWebSocketMessage({ action: 'get_schedules' });
}

function renderCalendar() {
    if (!calendarBody) return;

    // Clear existing calendar
    calendarBody.innerHTML = '';

    // Generate hourly rows (6 AM to 11 PM)
    const startHour = 6;
    const endHour = 23;

    for (let hour = startHour; hour <= endHour; hour++) {
        const row = document.createElement('div');
        row.className = 'calendar-row';
        row.style.display = 'contents'; // Make children part of parent grid

        // Time cell
        const timeCell = document.createElement('div');
        timeCell.className = 'time-cell';
        timeCell.textContent = `${hour.toString().padStart(2, '0')}:00`;
        row.appendChild(timeCell);

        // Day cells (Sunday = 0 to Saturday = 6)
        for (let day = 0; day < 7; day++) {
            const dayCell = document.createElement('div');
            dayCell.className = 'calendar-cell';
            dayCell.dataset.day = day;
            dayCell.dataset.hour = hour;

            // Find schedules for this day/hour
            const cellSchedules = schedules.filter(s => {
                return s.dayOfWeek === day && s.startHour === hour;
            });

            // Render schedule blocks in this cell
            cellSchedules.forEach(schedule => {
                const block = document.createElement('div');
                block.className = 'schedule-block';
                if (!schedule.enabled) {
                    block.classList.add('schedule-disabled');
                }
                // Use textContent for safety (automatically escapes)
                block.textContent = `${schedule.clubName} (${schedule.durationMinutes}m)`;
                // Title attribute is automatically escaped by browser
                block.title = `${schedule.clubName}\n${getDayName(schedule.dayOfWeek)} ${schedule.startHour.toString().padStart(2, '0')}:${schedule.startMinute.toString().padStart(2, '0')}\n${schedule.durationMinutes} minutes\nOwner: ${schedule.ownerUsername}`;
                block.dataset.scheduleId = schedule.id;
                block.addEventListener('click', (e) => {
                    e.stopPropagation();
                    editSchedule(schedule.id);
                });
                dayCell.appendChild(block);
            });

            // Allow clicking empty cells to add new schedule
            if (userRole !== 'viewer') {
                dayCell.addEventListener('click', () => {
                    quickAddSchedule(day, hour);
                });
            }

            row.appendChild(dayCell);
        }

        calendarBody.appendChild(row);
    }
}

function renderScheduleList() {
    if (!schedulesList) return;

    // Filter schedules by club if filter is active
    let filteredSchedules = schedules;
    if (scheduleClubFilter && scheduleClubFilter.value.trim()) {
        const filterText = scheduleClubFilter.value.trim().toLowerCase();
        filteredSchedules = schedules.filter(s =>
            s.clubName.toLowerCase().includes(filterText)
        );
    }

    if (filteredSchedules.length === 0) {
        schedulesList.innerHTML = '<p class="text-secondary">No schedules found</p>';
        return;
    }

    // Sort by day, then by time
    filteredSchedules.sort((a, b) => {
        if (a.dayOfWeek !== b.dayOfWeek) return a.dayOfWeek - b.dayOfWeek;
        if (a.startHour !== b.startHour) return a.startHour - b.startHour;
        return a.startMinute - b.startMinute;
    });

    let html = '';
    filteredSchedules.forEach(schedule => {
        const timeStr = `${schedule.startHour.toString().padStart(2, '0')}:${schedule.startMinute.toString().padStart(2, '0')}`;
        const enabledBadge = schedule.enabled
            ? '<span class="badge badge-success">Enabled</span>'
            : '<span class="badge badge-secondary">Disabled</span>';

        // Escape user-provided data
        const safeClubName = escapeHtml(schedule.clubName);
        const safeOwner = escapeHtml(schedule.ownerUsername);
        const safeId = escapeHtml(schedule.id);

        html += `
            <div class="schedule-item" data-schedule-id="${safeId}">
                <div class="schedule-item-header">
                    <h4>${safeClubName}</h4>
                    ${enabledBadge}
                </div>
                <div class="schedule-item-details">
                    <span><strong>${getDayName(schedule.dayOfWeek)}</strong> at ${timeStr}</span>
                    <span>${schedule.durationMinutes} minutes</span>
                    <span>Owner: ${safeOwner}</span>
                </div>
                <div class="schedule-item-actions">
                    <button class="btn btn-secondary btn-small" onclick="editSchedule('${safeId}')">Edit</button>
                    <button class="btn btn-danger btn-small" onclick="deleteSchedule('${safeId}')">Delete</button>
                </div>
            </div>
        `;
    });

    schedulesList.innerHTML = html;
}

function getDayName(day) {
    const days = ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'];
    return days[day] || 'Unknown';
}

function quickAddSchedule(day, hour) {
    // Pre-fill form with clicked day/hour
    if (scheduleDay) scheduleDay.value = day;
    if (scheduleHour) scheduleHour.value = hour;
    if (scheduleMinute) scheduleMinute.value = 0;
    if (scheduleDuration) scheduleDuration.value = 60;
    if (scheduleEnabled) scheduleEnabled.checked = true;
    if (scheduleClubName) scheduleClubName.value = '';

    editingScheduleId = null;

    // Scroll to form
    const scheduleForm = document.querySelector('#schedule-page .settings-card');
    if (scheduleForm) {
        scheduleForm.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
        if (scheduleClubName) scheduleClubName.focus();
    }
}

function editSchedule(scheduleId) {
    const schedule = schedules.find(s => s.id === scheduleId);
    if (!schedule) return;

    // Check permission
    if (userRole === 'viewer') {
        showTemporaryMessage('You need to be logged in to edit schedules', 'error');
        return;
    }

    if (userRole === 'operator' && schedule.ownerUsername !== currentUsername) {
        showTemporaryMessage('You can only edit schedules you created', 'error');
        return;
    }

    // Fill form
    if (scheduleClubName) scheduleClubName.value = schedule.clubName;
    if (scheduleDay) scheduleDay.value = schedule.dayOfWeek;
    if (scheduleHour) scheduleHour.value = schedule.startHour;
    if (scheduleMinute) scheduleMinute.value = schedule.startMinute;
    if (scheduleDuration) scheduleDuration.value = schedule.durationMinutes;
    if (scheduleEnabled) scheduleEnabled.checked = schedule.enabled;

    editingScheduleId = scheduleId;

    // Update form title
    const formTitle = document.getElementById('schedule-form-title');
    if (formTitle) {
        formTitle.textContent = 'Edit Schedule';
    }

    // Scroll to form
    const scheduleForm = document.querySelector('#schedule-page .settings-card');
    if (scheduleForm) {
        scheduleForm.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
    }
}

function deleteSchedule(scheduleId) {
    const schedule = schedules.find(s => s.id === scheduleId);
    if (!schedule) return;

    // Check permission
    if (userRole === 'viewer') {
        showTemporaryMessage('You need to be logged in to delete schedules', 'error');
        return;
    }

    if (userRole === 'operator' && schedule.ownerUsername !== currentUsername) {
        showTemporaryMessage('You can only delete schedules you created', 'error');
        return;
    }

    if (confirm(`Delete schedule for ${schedule.clubName} on ${getDayName(schedule.dayOfWeek)}?`)) {
        sendWebSocketMessage({
            action: 'delete_schedule',
            id: scheduleId
        });
    }
}

function saveSchedule() {
    // Validate inputs
    const clubName = scheduleClubName?.value.trim();
    const day = parseInt(scheduleDay?.value);
    const hour = parseInt(scheduleHour?.value);
    const minute = parseInt(scheduleMinute?.value);
    const duration = parseInt(scheduleDuration?.value);
    const enabled = scheduleEnabled?.checked;

    if (!clubName) {
        showTemporaryMessage('Please enter a club name', 'error');
        return;
    }

    if (isNaN(day) || day < 0 || day > 6) {
        showTemporaryMessage('Please select a valid day', 'error');
        return;
    }

    if (isNaN(hour) || hour < 0 || hour > 23) {
        showTemporaryMessage('Please enter a valid hour (0-23)', 'error');
        return;
    }

    if (isNaN(minute) || minute < 0 || minute > 59) {
        showTemporaryMessage('Please enter a valid minute (0-59)', 'error');
        return;
    }

    if (isNaN(duration) || duration < 1 || duration > 180) {
        showTemporaryMessage('Please enter a valid duration (1-180 minutes)', 'error');
        return;
    }

    const scheduleData = {
        clubName: clubName,
        dayOfWeek: day,
        startHour: hour,
        startMinute: minute,
        durationMinutes: duration,
        enabled: enabled !== false
    };

    if (editingScheduleId) {
        // Update existing schedule
        scheduleData.id = editingScheduleId;
        sendWebSocketMessage({
            action: 'update_schedule',
            schedule: scheduleData
        });
    } else {
        // Add new schedule
        sendWebSocketMessage({
            action: 'add_schedule',
            schedule: scheduleData
        });
    }

    // Clear form
    cancelScheduleEdit();
}

function cancelScheduleEdit() {
    if (scheduleClubName) scheduleClubName.value = '';
    if (scheduleDay) scheduleDay.value = 0;
    if (scheduleHour) scheduleHour.value = '';
    if (scheduleMinute) scheduleMinute.value = '';
    if (scheduleDuration) scheduleDuration.value = 60;
    if (scheduleEnabled) scheduleEnabled.checked = true;

    editingScheduleId = null;

    const formTitle = document.getElementById('schedule-form-title');
    if (formTitle) {
        formTitle.textContent = 'Add New Schedule';
    }
}

// --- Hello Club Functions ---

function showHelloClubPage(show) {
    mainPage.classList.toggle('hidden', show);
    settingsPage.classList.add('hidden');
    userManagementPage.classList.add('hidden');
    schedulePage.classList.add('hidden');
    if (helloClubPage) {
        helloClubPage.classList.toggle('hidden', !show);
    }

    if (show) {
        loadHelloClubSettings();
    }
}

function loadHelloClubSettings() {
    sendWebSocketMessage({ action: 'get_helloclub_settings' });
}

function saveHelloClubSettings() {
    const apiKey = helloClubApiKey?.value.trim();
    const enabled = helloClubEnabled?.checked;
    const daysAhead = parseInt(helloClubDaysAhead?.value);
    const syncHour = parseInt(helloClubSyncHour?.value);

    // Build category filter from selected categories
    const categoryFilter = selectedCategories.join(',');

    const settings = {
        enabled: enabled !== false,
        daysAhead: isNaN(daysAhead) ? 7 : daysAhead,
        categoryFilter: categoryFilter,
        syncHour: isNaN(syncHour) ? 0 : syncHour
    };

    // Only include API key if it's been changed
    if (apiKey && apiKey !== '***configured***') {
        settings.apiKey = apiKey;
    }

    sendWebSocketMessage({
        action: 'save_helloclub_settings',
        ...settings
    });
}

function fetchHelloClubCategories() {
    showLoadingOverlay('Fetching categories from Hello Club...');
    sendWebSocketMessage({ action: 'get_helloclub_categories' });
}

function renderHelloClubCategories(categories) {
    if (!helloClubCategoriesList) return;

    if (categories.length === 0) {
        helloClubCategoriesList.innerHTML = '<p class="text-secondary">No categories found</p>';
        return;
    }

    let html = '';
    categories.forEach(category => {
        const isChecked = selectedCategories.includes(category);
        const safeCategory = escapeHtml(category);
        html += `
            <div class="category-checkbox-item">
                <input type="checkbox" id="cat-${safeCategory}" value="${safeCategory}" ${isChecked ? 'checked' : ''}
                       onchange="toggleCategory('${safeCategory}')">
                <label for="cat-${safeCategory}">${safeCategory}</label>
            </div>
        `;
    });

    helloClubCategoriesList.innerHTML = html;
}

function toggleCategory(category) {
    const index = selectedCategories.indexOf(category);
    if (index > -1) {
        selectedCategories.splice(index, 1);
    } else {
        selectedCategories.push(category);
    }
}

function previewHelloClubEvents() {
    showLoadingOverlay('Loading events from Hello Club...');
    sendWebSocketMessage({ action: 'get_helloclub_events' });
}

function renderHelloClubEvents(events) {
    if (!helloClubEventsContainer) return;

    if (events.length === 0) {
        helloClubEventsContainer.innerHTML = '<p class="text-secondary">No events found</p>';
        return;
    }

    let html = '';
    events.forEach(event => {
        const hasConflict = event.hasConflict || false;
        const conflictClass = hasConflict ? 'has-conflict' : '';

        // Parse date for display
        const startDate = new Date(event.startDate);
        const dayNames = ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'];
        const dayName = dayNames[startDate.getDay()];
        const timeStr = startDate.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit', hour12: true });
        const dateStr = startDate.toLocaleDateString('en-US', { month: 'short', day: 'numeric', year: 'numeric' });

        // Escape user-provided data
        const safeName = escapeHtml(event.name);
        const safeId = escapeHtml(event.id);
        const safeCategory = escapeHtml(event.categoryName);
        const safeConflictWith = escapeHtml(event.conflictWith);

        html += `
            <div class="event-item ${conflictClass}">
                <div class="event-checkbox-header">
                    <input type="checkbox" id="event-${safeId}" value="${safeId}"
                           ${hasConflict ? '' : 'checked'} class="event-checkbox">
                    <div class="event-info">
                        <div class="event-name">${safeName}</div>
                        <div class="event-details">
                            <div class="event-detail-row">
                                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                                    <rect x="3" y="4" width="18" height="18" rx="2" ry="2"></rect>
                                    <line x1="16" y1="2" x2="16" y2="6"></line>
                                    <line x1="8" y1="2" x2="8" y2="6"></line>
                                    <line x1="3" y1="10" x2="21" y2="10"></line>
                                </svg>
                                <span>${dayName}, ${dateStr} at ${timeStr}</span>
                            </div>
                            <div class="event-detail-row">
                                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                                    <circle cx="12" cy="12" r="10"></circle>
                                    <polyline points="12 6 12 12 16 14"></polyline>
                                </svg>
                                <span>${event.durationMinutes} minutes</span>
                            </div>
                            ${event.categoryName ? `
                            <div class="event-detail-row">
                                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                                    <path d="M20 21v-2a4 4 0 0 0-4-4H8a4 4 0 0 0-4 4v2"></path>
                                    <circle cx="12" cy="7" r="4"></circle>
                                </svg>
                                <span>${safeCategory}</span>
                            </div>
                            ` : ''}
                        </div>
                    </div>
                </div>
                ${hasConflict ? `
                <div class="event-conflict-warning">
                    <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                        <path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"></path>
                        <line x1="12" y1="9" x2="12" y2="13"></line>
                        <line x1="12" y1="17" x2="12.01" y2="17"></line>
                    </svg>
                    <span>Conflicts with existing schedule: ${safeConflictWith || 'Unknown'}</span>
                </div>
                ` : ''}
            </div>
        `;
    });

    helloClubEventsContainer.innerHTML = html;
}

function importSelectedHelloClubEvents() {
    const checkboxes = document.querySelectorAll('.event-checkbox:checked');
    const eventIds = Array.from(checkboxes).map(cb => cb.value);

    if (eventIds.length === 0) {
        showTemporaryMessage('Please select at least one event to import', 'error');
        return;
    }

    showLoadingOverlay(`Importing ${eventIds.length} event(s)...`);
    sendWebSocketMessage({
        action: 'import_helloclub_events',
        eventIds: eventIds
    });
}

function syncHelloClubNow() {
    if (confirm('This will automatically import all matching events. Continue?')) {
        showLoadingOverlay('Syncing with Hello Club...');
        sendWebSocketMessage({ action: 'sync_helloclub_now' });
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

function updateNTPStatus(data) {
    if (!ntpStatusElement) return;

    if (data.synced) {
        ntpStatusElement.className = 'ntp-status ntp-synced';
        ntpStatusElement.textContent = '✓';

        // Build tooltip with detailed info
        let tooltip = 'Time synced via NTP';
        if (data.timezone) {
            tooltip += `\nTimezone: ${data.timezone}`;

            // Update timezone dropdown if present
            const timezoneSelect = document.getElementById('timezone-select');
            if (timezoneSelect) {
                timezoneSelect.value = data.timezone;
            }
        }
        if (data.autoSyncInterval) {
            tooltip += `\nAuto-sync: every ${data.autoSyncInterval} min`;
        }
        ntpStatusElement.title = tooltip;
    } else {
        ntpStatusElement.className = 'ntp-status ntp-error';
        ntpStatusElement.textContent = '✗';
        ntpStatusElement.title = 'Time not synced - schedules may not work correctly';
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

    // Timezone dropdown (admin only)
    const timezoneSelect = document.getElementById('timezone-select');
    if (timezoneSelect) {
        timezoneSelect.addEventListener('change', () => {
            if (userRole !== 'admin') {
                showTemporaryMessage('Only administrators can change timezone', 'error');
                return;
            }

            const newTimezone = timezoneSelect.value;
            if (confirm(`Change timezone to ${newTimezone}?\n\nThis will affect all scheduled events and time display.`)) {
                sendWebSocketMessage({
                    action: 'set_timezone',
                    timezone: newTimezone
                });
            } else {
                // Revert to current value from NTP status
                updateNTPStatus({ synced: true, timezone: timezoneSelect.dataset.currentTimezone || 'Pacific/Auckland' });
            }
        });
    }

    // --- Authentication event listeners ---
    if (loginBtn) {
        loginBtn.addEventListener('click', () => {
            const username = loginUsernameInput?.value.trim();
            const password = loginPasswordInput?.value.trim();

            if (!username || !password) {
                showTemporaryMessage('Please enter username and password', 'error');
                return;
            }

            sendWebSocketMessage({
                action: 'authenticate',
                username: username,
                password: password
            });
        });
    }

    if (viewerBtn) {
        viewerBtn.addEventListener('click', () => {
            // Continue as viewer (no auth needed)
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

            sendWebSocketMessage({
                action: 'add_operator',
                username: username,
                password: password
            });
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

            sendWebSocketMessage({
                action: 'change_password',
                username: currentUsername,
                oldPassword: oldPassword,
                newPassword: newPassword
            });
        });
    }

    if (factoryResetBtn) {
        factoryResetBtn.addEventListener('click', () => {
            if (confirm('Factory reset will restore all user accounts to defaults. This cannot be undone. Continue?')) {
                sendWebSocketMessage({ action: 'factory_reset' });
            }
        });
    }

    // --- Schedule event listeners ---
    if (scheduleIcon) {
        scheduleIcon.addEventListener('click', () => {
            if (userRole === 'viewer') {
                showTemporaryMessage('Please login to access schedule management', 'error');
                loginModal.classList.remove('hidden');
            } else {
                showSchedulePage(true);
            }
        });
    }

    if (closeScheduleBtn) {
        closeScheduleBtn.addEventListener('click', () => showSchedulePage(false));
    }

    if (schedulingEnabledToggle) {
        schedulingEnabledToggle.addEventListener('change', () => {
            sendWebSocketMessage({
                action: 'enable_scheduling',
                enabled: schedulingEnabledToggle.checked
            });
        });
    }

    if (loadSchedulesBtn) {
        loadSchedulesBtn.addEventListener('click', loadSchedules);
    }

    if (scheduleClubFilter) {
        scheduleClubFilter.addEventListener('input', () => {
            renderScheduleList();
        });
    }

    if (saveScheduleBtn) {
        saveScheduleBtn.addEventListener('click', saveSchedule);
    }

    if (cancelScheduleBtn) {
        cancelScheduleBtn.addEventListener('click', cancelScheduleEdit);
    }

    // --- Hello Club event listeners ---
    if (helloClubIcon) {
        helloClubIcon.addEventListener('click', () => {
            if (userRole === 'admin') {
                showHelloClubPage(true);
            } else {
                showTemporaryMessage('Admin access required', 'error');
            }
        });
    }

    if (closeHelloClubBtn) {
        closeHelloClubBtn.addEventListener('click', () => showHelloClubPage(false));
    }

    if (saveHelloClubSettingsBtn) {
        saveHelloClubSettingsBtn.addEventListener('click', saveHelloClubSettings);
    }

    if (fetchCategoriesBtn) {
        fetchCategoriesBtn.addEventListener('click', fetchHelloClubCategories);
    }

    if (previewEventsBtn) {
        previewEventsBtn.addEventListener('click', () => {
            previewHelloClubEvents();
            if (helloClubImportModal) {
                helloClubImportModal.classList.remove('hidden');
            }
        });
    }

    if (syncNowBtn) {
        syncNowBtn.addEventListener('click', syncHelloClubNow);
    }

    if (importSelectedBtn) {
        importSelectedBtn.addEventListener('click', importSelectedHelloClubEvents);
    }

    if (cancelImportBtn) {
        cancelImportBtn.addEventListener('click', () => {
            if (helloClubImportModal) {
                helloClubImportModal.classList.add('hidden');
            }
        });
    }

    // --- Keyboard support for login ---
    if (loginPasswordInput) {
        loginPasswordInput.addEventListener('keypress', (e) => {
            if (e.key === 'Enter') {
                loginBtn?.click();
            }
        });
    }

    if (loginUsernameInput) {
        loginUsernameInput.addEventListener('keypress', (e) => {
            if (e.key === 'Enter') {
                loginBtn?.click();
            }
        });
    }

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
    socket = new WebSocket(`ws://${window.location.host}/ws`);

    socket.onopen = () => {
        console.log('WebSocket connected');
        hideLoadingOverlay();
        reconnectAttempts = 0; // Reset on successful connection
        updateConnectionStatus(true);
        // Show login modal after connection
        if (loginModal) {
            loginModal.classList.remove('hidden');
        }
    };

    socket.onclose = () => {
        console.log('WebSocket disconnected');
        updateConnectionStatus(false);
        stopClientTimer();

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
                // Show login modal on connection
                loginModal.classList.remove('hidden');
                break;

            case 'auth_success':
                userRole = data.role || 'viewer';
                currentUsername = data.username || 'Viewer';
                loginModal.classList.add('hidden');
                updateUIForRole();
                showTemporaryMessage(`Welcome, ${currentUsername}!`, "success");
                break;

            case 'auth_failed':
                showTemporaryMessage(data.message || "Authentication failed", "error");
                loginModal.classList.remove('hidden');
                break;

            case 'operators_list':
                renderOperatorsList(data.operators || []);
                break;

            case 'operator_added':
                showTemporaryMessage(`Operator "${data.username}" added successfully`, "success");
                if (newOperatorUsernameInput) newOperatorUsernameInput.value = '';
                if (newOperatorPasswordInput) newOperatorPasswordInput.value = '';
                loadOperators();
                break;

            case 'operator_removed':
                showTemporaryMessage(`Operator "${data.username}" removed`, "success");
                loadOperators();
                break;

            case 'password_changed':
                showTemporaryMessage("Password changed successfully", "success");
                if (changeOldPasswordInput) changeOldPasswordInput.value = '';
                if (changeNewPasswordInput) changeNewPasswordInput.value = '';
                break;

            case 'timezone_changed':
                showTemporaryMessage(`Timezone updated to ${data.timezone}`, "success");
                // Store current timezone in dataset for reverting if needed
                const timezoneSelect = document.getElementById('timezone-select');
                if (timezoneSelect) {
                    timezoneSelect.dataset.currentTimezone = data.timezone;
                }
                break;

            case 'factory_reset_complete':
                showTemporaryMessage("Factory reset complete. Please refresh page.", "success");
                setTimeout(() => window.location.reload(), 2000);
                break;

            case 'schedules_list':
                schedules = data.schedules || [];
                schedulingEnabled = data.schedulingEnabled || false;
                if (schedulingEnabledToggle) {
                    schedulingEnabledToggle.checked = schedulingEnabled;
                }
                renderCalendar();
                renderScheduleList();
                break;

            case 'schedule_added':
                showTemporaryMessage(`Schedule for "${data.schedule.clubName}" added`, "success");
                loadSchedules();
                break;

            case 'schedule_updated':
                showTemporaryMessage(`Schedule for "${data.schedule.clubName}" updated`, "success");
                loadSchedules();
                break;

            case 'schedule_deleted':
                showTemporaryMessage("Schedule deleted", "success");
                loadSchedules();
                break;

            case 'schedule_started':
                const schedule = data.schedule;
                showTemporaryMessage(`Timer started by schedule: ${schedule.clubName}`, "success");
                break;

            case 'scheduling_enabled':
                schedulingEnabled = data.enabled;
                if (schedulingEnabledToggle) {
                    schedulingEnabledToggle.checked = schedulingEnabled;
                }
                showTemporaryMessage(
                    schedulingEnabled ? "Scheduling enabled" : "Scheduling disabled",
                    "success"
                );
                break;

            case 'ntp_status':
                updateNTPStatus(data);
                break;

            case 'helloclub_settings':
                // Populate settings form
                if (helloClubApiKey) {
                    helloClubApiKey.value = data.apiKey || '';
                }
                if (helloClubEnabled) {
                    helloClubEnabled.checked = data.enabled || false;
                }
                if (helloClubDaysAhead) {
                    helloClubDaysAhead.value = data.daysAhead || 7;
                }
                if (helloClubSyncHour) {
                    helloClubSyncHour.value = data.syncHour || 0;
                }
                // Parse category filter
                if (data.categoryFilter) {
                    selectedCategories = data.categoryFilter.split(',').filter(c => c.trim());
                } else {
                    selectedCategories = [];
                }
                // Update last sync display
                if (lastSyncText && data.lastSyncDay >= 0) {
                    lastSyncText.textContent = `Last synced on day ${data.lastSyncDay}`;
                } else if (lastSyncText) {
                    lastSyncText.textContent = 'Never synced';
                }
                break;

            case 'helloclub_settings_saved':
                showTemporaryMessage(data.message || 'Hello Club settings saved', 'success');
                break;

            case 'helloclub_categories':
                hideLoadingOverlay();
                helloClubCategories = data.categories || [];
                renderHelloClubCategories(helloClubCategories);
                break;

            case 'helloclub_events':
                hideLoadingOverlay();
                helloClubEvents = data.events || [];
                renderHelloClubEvents(helloClubEvents);
                break;

            case 'helloclub_import_complete':
                hideLoadingOverlay();
                if (helloClubImportModal) {
                    helloClubImportModal.classList.add('hidden');
                }
                showTemporaryMessage(
                    `Import complete: ${data.imported} imported, ${data.skipped} skipped`,
                    'success'
                );
                // Reload schedules if we're on schedule page
                if (!schedulePage.classList.contains('hidden')) {
                    loadSchedules();
                }
                break;

            case 'helloclub_sync_complete':
                hideLoadingOverlay();
                showTemporaryMessage(data.message || 'Hello Club sync completed', 'success');
                // Reload schedules if we're on schedule page
                if (!schedulePage.classList.contains('hidden')) {
                    loadSchedules();
                }
                // Update last sync text
                if (lastSyncText) {
                    const now = new Date();
                    lastSyncText.textContent = `Last synced: ${now.toLocaleString()}`;
                }
                break;

            case 'error':
                hideLoadingOverlay();
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

// --- Make functions globally available for onclick handlers ---
window.editSchedule = editSchedule;
window.deleteSchedule = deleteSchedule;
window.removeOperator = removeOperator;
window.toggleCategory = toggleCategory;

// --- Main Execution ---
if (!SIMULATION_MODE) {
    initializeEventListeners();
    connectWebSocket();
} else {
    console.log("Running in Simulation Mode");
    initializeEventListeners();
}
