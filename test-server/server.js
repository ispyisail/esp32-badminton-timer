const express = require('express');
const WebSocket = require('ws');
const path = require('path');
const https = require('https');
const os = require('os');

const app = express();
const PORT = 8080;

// Get LAN IP address
function getLanIp() {
    const interfaces = os.networkInterfaces();
    for (const name of Object.keys(interfaces)) {
        for (const iface of interfaces[name]) {
            if (iface.family === 'IPv4' && !iface.internal) {
                return iface.address;
            }
        }
    }
    return 'localhost';
}
const lanIp = getLanIp();

// Serve static files from data directory
app.use(express.static(path.join(__dirname, '../data')));

const server = app.listen(PORT, () => {
    console.log(`\n🚀 Test Server running!`);
    console.log(`📱 Local:   http://localhost:${PORT}`);
    console.log(`📱 Network: http://${lanIp}:${PORT}`);
    console.log(`\n✨ Test credentials:`);
    console.log(`   Admin: username="admin", password="admin"`);
    console.log(`   Operator: username="operator1", password="pass123"`);
    console.log(`   Viewer: leave both fields empty\n`);
});

// WebSocket server
const wss = new WebSocket.Server({ server });

// --- State ---
let timerStatus = 'IDLE';
let mainTimerRemaining = 0;
let currentRound = 1;
let pauseAfterNext = false;
let continuousMode = false;
let activeEventEndTime = 0;
let activeEventName = '';

let settings = {
    gameDuration: 12 * 60 * 1000,
    numRounds: 3,
    sirenLength: 1000,
    sirenPause: 1000
};

let operators = [
    { username: "operator1", password: "pass123" },
    { username: "operator2", password: "test456" }
];

let currentTimezone = 'Pacific/Auckland';

// Hello Club config
let hcApiKey = '';       // Actual API key (stored in memory for testing)
let hcEnabled = false;
let hcDefaultDuration = 12;

// QR config
let qrConfig = {
    ssidOverride: '',
    connectedSsid: 'Gargoyle',
    password: '',
    encryption: 'WPA'
};

// Cached events (populated from real API or mock fallback)
let cachedEvents = [];
let lastHCSync = 0;

// ==========================================================================
// Hello Club API — Real HTTP client
// ==========================================================================

const HC_BASE_URL = 'https://api.helloclub.com';
const HC_DAYS_AHEAD = 7;
const HC_TRIGGER_WINDOW_SEC = 120;

function parseTimerTag(description) {
    if (!description) return null;
    const lower = description.toLowerCase();
    const tagPos = lower.indexOf('timer:');
    if (tagPos === -1) return null;

    let value = description.substring(tagPos + 6).trim();
    const nlPos = value.indexOf('\n');
    if (nlPos > 0) value = value.substring(0, nlPos);
    value = value.trim();
    const lowerValue = value.toLowerCase();

    let duration = hcDefaultDuration;
    let rounds = 0; // 0 = continuous

    if (lowerValue.startsWith('enabled') || value === '') {
        return { duration, rounds };
    }

    // Parse Xmin
    const minMatch = lowerValue.match(/(\d+)\s*min/);
    if (minMatch) {
        const val = parseInt(minMatch[1]);
        if (val >= 1 && val <= 120) duration = val;
    }

    // Parse Xrounds (optional)
    const roundMatch = lowerValue.match(/(\d+)\s*round/);
    if (roundMatch) {
        const val = parseInt(roundMatch[1]);
        if (val >= 1 && val <= 20) rounds = val;
    }

    return { duration, rounds };
}

function hcApiFetch(endpoint, params) {
    return new Promise((resolve, reject) => {
        if (!hcApiKey) {
            reject(new Error('API key not configured'));
            return;
        }

        let url = `${HC_BASE_URL}${endpoint}`;
        if (params) url += '?' + params;

        const urlObj = new URL(url);
        const options = {
            hostname: urlObj.hostname,
            path: urlObj.pathname + urlObj.search,
            method: 'GET',
            headers: {
                'X-Api-Key': hcApiKey,
                'Content-Type': 'application/json',
                'Accept': 'application/json'
            }
        };

        const req = https.request(options, (res) => {
            let data = '';
            res.on('data', chunk => data += chunk);
            res.on('end', () => {
                if (res.statusCode === 200) {
                    try {
                        resolve(JSON.parse(data));
                    } catch (e) {
                        reject(new Error(`JSON parse error: ${e.message}`));
                    }
                } else if (res.statusCode === 401) {
                    reject(new Error('Invalid API key (401)'));
                } else {
                    reject(new Error(`HTTP ${res.statusCode}`));
                }
            });
        });

        req.on('error', (e) => reject(new Error(`Request failed: ${e.message}`)));
        req.setTimeout(10000, () => { req.destroy(); reject(new Error('Request timeout')); });
        req.end();
    });
}

async function fetchAndCacheEvents() {
    if (!hcApiKey) {
        console.log('📅 No API key — skipping real fetch');
        return false;
    }

    try {
        const now = new Date();
        const future = new Date(now.getTime() + HC_DAYS_AHEAD * 24 * 60 * 60 * 1000);
        const fromDate = now.toISOString();
        const toDate = future.toISOString();

        const params = `fromDate=${fromDate}&toDate=${toDate}&sort=startDate&limit=50`;
        console.log(`🌐 Fetching Hello Club events...`);

        const response = await hcApiFetch('/event', params);
        const events = response.events || response;

        if (!Array.isArray(events)) {
            console.log('⚠️  Unexpected API response format');
            return false;
        }

        console.log(`🌐 API returned ${events.length} events`);

        // Build new event list, preserving triggered state
        const oldTriggered = new Map(cachedEvents.map(e => [e.id, e.triggered]));
        const newEvents = [];

        for (const event of events) {
            const description = event.description || '';
            const parsed = parseTimerTag(description);
            if (!parsed) continue; // Skip events without timer: tag

            const id = (event.id || '').substring(0, 12);
            const name = (event.name || 'Unnamed').substring(0, 40);
            const startTime = Math.floor(new Date(event.startDate).getTime() / 1000);
            const endTime = Math.floor(new Date(event.endDate).getTime() / 1000);

            if (isNaN(startTime) || isNaN(endTime)) continue;

            newEvents.push({
                id,
                name,
                startTime,
                endTime,
                durationMin: parsed.duration,
                numRounds: parsed.rounds,
                triggered: oldTriggered.get(id) || false
            });

            console.log(`  ✅ ${name} — ${parsed.duration}min${parsed.rounds ? ' ' + parsed.rounds + 'rounds' : ' continuous'}`);

            if (newEvents.length >= 20) break;
        }

        cachedEvents = newEvents;
        lastHCSync = Date.now();
        console.log(`📅 Cached ${cachedEvents.length} events with timer: tag`);
        return true;

    } catch (err) {
        console.error(`❌ Hello Club API error: ${err.message}`);
        return false;
    }
}

// Purge expired events
function purgeExpiredEvents() {
    const nowSec = Math.floor(Date.now() / 1000);
    const before = cachedEvents.length;
    cachedEvents = cachedEvents.filter(e => e.endTime >= nowSec);
    if (cachedEvents.length < before) {
        console.log(`🗑️  Purged ${before - cachedEvents.length} expired events`);
    }
}

// ==========================================================================
// Client tracking & messaging
// ==========================================================================

const clients = new Map();
let nextClientId = 1;

function getFormattedTime() {
    try {
        return new Date().toLocaleTimeString('en-US', {
            hour: '2-digit', minute: '2-digit', second: '2-digit',
            hour12: true, timeZone: currentTimezone
        });
    } catch (err) {
        return new Date().toLocaleTimeString('en-US', {
            hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: true
        });
    }
}

function sendMessage(ws, data) {
    if (ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(data));
}

function sendError(ws, message) {
    sendMessage(ws, { event: 'error', message });
}

function broadcast(data) {
    clients.forEach(client => sendMessage(client.ws, data));
}

function buildSyncMessage() {
    return {
        event: 'sync',
        status: timerStatus,
        mainTimerRemaining,
        currentRound,
        numRounds: settings.numRounds,
        pauseAfterNext,
        continuousMode,
        activeEventEndTime,
        activeEventName
    };
}

// ==========================================================================
// WebSocket connections
// ==========================================================================

wss.on('connection', (ws) => {
    const clientId = nextClientId++;
    clients.set(clientId, { ws, role: 'viewer', username: 'Viewer' });
    console.log(`✅ Client ${clientId} connected`);

    sendMessage(ws, { event: 'settings', settings });
    sendMessage(ws, {
        event: 'state',
        state: {
            status: timerStatus,
            time: getFormattedTime(),
            currentRound,
            numRounds: settings.numRounds,
            mainTimer: mainTimerRemaining,
            pauseAfterNext,
            continuousMode,
            activeEventEndTime,
            activeEventName
        }
    });
    sendMessage(ws, buildSyncMessage());
    sendMessage(ws, {
        event: 'ntp_status',
        synced: true,
        time: getFormattedTime(),
        timezone: currentTimezone,
        autoSyncInterval: 30
    });

    ws.on('message', (data) => {
        try {
            const msg = JSON.parse(data);
            handleMessage(clientId, ws, msg);
        } catch (err) {
            console.error('Error parsing message:', err);
        }
    });

    ws.on('close', () => {
        console.log(`❌ Client ${clientId} disconnected`);
        clients.delete(clientId);
    });
});

// ==========================================================================
// Message handler
// ==========================================================================

function handleMessage(clientId, ws, msg) {
    const client = clients.get(clientId);
    console.log(`📨 Client ${clientId} [${client.username}/${client.role}]: ${msg.action}`);

    switch (msg.action) {
        case 'authenticate':
            handleAuth(clientId, ws, msg);
            break;

        case 'start':
            if (client.role === 'viewer') { sendError(ws, 'Permission denied'); return; }
            timerStatus = 'RUNNING';
            mainTimerRemaining = settings.gameDuration;
            currentRound = 1;
            pauseAfterNext = false;
            continuousMode = false;
            broadcast({
                event: 'start',
                status: 'RUNNING',
                gameDuration: settings.gameDuration,
                currentRound,
                numRounds: settings.numRounds,
                pauseAfterNext,
                continuousMode,
                activeEventEndTime,
                activeEventName
            });
            console.log('⏱️  Timer started');
            break;

        case 'pause':
            if (client.role === 'viewer') { sendError(ws, 'Permission denied'); return; }
            if (timerStatus === 'RUNNING') {
                timerStatus = 'PAUSED';
                broadcast({ event: 'pause' });
                console.log('⏸️  Timer paused');
            } else if (timerStatus === 'PAUSED') {
                timerStatus = 'RUNNING';
                broadcast({ event: 'resume' });
                console.log('▶️  Timer resumed');
            }
            break;

        case 'pause_after_next':
            if (client.role === 'viewer') { sendError(ws, 'Permission denied'); return; }
            pauseAfterNext = msg.enabled === true;
            broadcast(buildSyncMessage());
            console.log(`⏭️  Pause after next: ${pauseAfterNext}`);
            break;

        case 'reset':
            if (client.role === 'viewer') { sendError(ws, 'Permission denied'); return; }
            timerStatus = 'IDLE';
            mainTimerRemaining = 0;
            currentRound = 1;
            pauseAfterNext = false;
            continuousMode = false;
            activeEventEndTime = 0;
            activeEventName = '';
            broadcast({ event: 'reset' });
            console.log('⏹️  Timer reset');
            break;

        case 'save_settings':
            if (client.role === 'viewer') { sendError(ws, 'Permission denied'); return; }
            if (msg.settings) settings = { ...settings, ...msg.settings };
            broadcast({ event: 'settings', settings });
            console.log('💾 Settings saved:', settings);
            break;

        case 'get_operators':
            if (client.role !== 'admin') { sendError(ws, 'Admin only'); return; }
            sendMessage(ws, { event: 'operators_list', operators: operators.map(o => o.username) });
            break;

        case 'add_operator':
            if (client.role !== 'admin') { sendError(ws, 'Admin only'); return; }
            operators.push({ username: msg.username, password: msg.password });
            sendMessage(ws, { event: 'operator_added', username: msg.username });
            console.log(`👤 Operator added: ${msg.username}`);
            break;

        case 'remove_operator':
            if (client.role !== 'admin') { sendError(ws, 'Admin only'); return; }
            operators = operators.filter(o => o.username !== msg.username);
            sendMessage(ws, { event: 'operator_removed', username: msg.username });
            console.log(`👤 Operator removed: ${msg.username}`);
            break;

        case 'change_password':
            handlePasswordChange(clientId, ws, msg);
            break;

        case 'set_timezone':
            if (client.role !== 'admin') { sendError(ws, 'Admin only'); return; }
            currentTimezone = msg.timezone || 'Pacific/Auckland';
            sendMessage(ws, { event: 'timezone_changed', timezone: currentTimezone });
            broadcast({ event: 'ntp_status', synced: true, time: getFormattedTime(), timezone: currentTimezone, autoSyncInterval: 30 });
            console.log(`🌍 Timezone changed to: ${currentTimezone}`);
            break;

        case 'factory_reset':
            if (client.role !== 'admin') { sendError(ws, 'Admin only'); return; }
            operators = [];
            currentTimezone = 'Pacific/Auckland';
            sendMessage(ws, { event: 'factory_reset_complete' });
            console.log('🔄 Factory reset completed');
            break;

        // Hello Club settings
        case 'get_helloclub_settings':
            if (client.role !== 'admin') { sendError(ws, 'Admin only'); return; }
            sendMessage(ws, {
                event: 'helloclub_settings',
                apiKey: hcApiKey ? '***configured***' : '',
                enabled: hcEnabled,
                defaultDuration: hcDefaultDuration
            });
            break;

        case 'save_helloclub_settings':
            if (client.role !== 'admin') { sendError(ws, 'Admin only'); return; }
            if (msg.apiKey && msg.apiKey !== '***configured***') {
                hcApiKey = msg.apiKey;
                console.log(`🔑 HC API key set (${hcApiKey.substring(0, 8)}...)`);
            }
            if (msg.enabled !== undefined) hcEnabled = msg.enabled;
            if (msg.defaultDuration !== undefined) hcDefaultDuration = msg.defaultDuration;
            sendMessage(ws, { event: 'helloclub_settings_saved', message: 'Hello Club settings saved successfully' });
            console.log(`💾 HC settings saved — key:${hcApiKey ? 'yes' : 'no'} enabled:${hcEnabled} defaultDur:${hcDefaultDuration}min`);

            // Auto-fetch events when API key is first set
            if (hcApiKey && hcEnabled) {
                fetchAndCacheEvents().then(success => {
                    if (success) {
                        broadcast({ event: 'upcoming_events', events: cachedEvents, lastSync: lastHCSync });
                    }
                });
            }
            break;

        // Upcoming events
        case 'get_upcoming_events':
            sendMessage(ws, {
                event: 'upcoming_events',
                events: cachedEvents,
                lastSync: lastHCSync
            });
            console.log(`📅 Upcoming events sent (${cachedEvents.length})`);
            break;

        case 'helloclub_refresh':
            if (client.role !== 'admin') { sendError(ws, 'Admin only'); return; }
            if (!hcApiKey) {
                sendMessage(ws, { event: 'helloclub_refresh_result', success: false, message: 'API key not configured' });
                return;
            }
            fetchAndCacheEvents().then(success => {
                if (success) {
                    sendMessage(ws, {
                        event: 'helloclub_refresh_result',
                        success: true,
                        eventCount: cachedEvents.length
                    });
                    broadcast({ event: 'upcoming_events', events: cachedEvents, lastSync: lastHCSync });
                    console.log(`🔄 Hello Club refreshed: ${cachedEvents.length} events`);
                } else {
                    sendMessage(ws, {
                        event: 'helloclub_refresh_result',
                        success: false,
                        message: 'Failed to fetch events from Hello Club'
                    });
                }
            });
            break;

        // QR
        case 'get_qr_config':
            sendMessage(ws, {
                event: 'qr_config',
                ssid: qrConfig.ssidOverride || qrConfig.connectedSsid,
                ssidOverride: qrConfig.ssidOverride,
                connectedSsid: qrConfig.connectedSsid,
                password: qrConfig.password,
                encryption: qrConfig.encryption,
                appUrl: `http://${lanIp}:${PORT}/`
            });
            break;

        case 'save_qr_settings':
            if (client.role !== 'admin') { sendError(ws, 'Admin only'); return; }
            qrConfig.ssidOverride = msg.ssid || '';
            qrConfig.password = msg.password || '';
            qrConfig.encryption = msg.encryption || 'WPA';
            sendMessage(ws, { event: 'qr_settings_saved' });
            console.log('💾 QR settings saved');
            break;

        default:
            console.log(`⚠️  Unknown action: ${msg.action}`);
    }
}

// ==========================================================================
// Auth
// ==========================================================================

function handleAuth(clientId, ws, msg) {
    const client = clients.get(clientId);

    if (!msg.username && !msg.password) {
        client.role = 'viewer';
        client.username = 'Viewer';
        sendMessage(ws, { event: 'auth_success', role: 'viewer', username: 'Viewer' });
        console.log(`👁️  Client ${clientId} viewing as guest`);
        return;
    }

    if (msg.username === 'admin' && msg.password === 'admin') {
        client.role = 'admin';
        client.username = 'admin';
        sendMessage(ws, { event: 'auth_success', role: 'admin', username: 'admin' });
        console.log(`🔑 Client ${clientId} logged in as ADMIN`);
        return;
    }

    const operator = operators.find(o => o.username === msg.username && o.password === msg.password);
    if (operator) {
        client.role = 'operator';
        client.username = operator.username;
        sendMessage(ws, { event: 'auth_success', role: 'operator', username: operator.username });
        console.log(`🔑 Client ${clientId} logged in as OPERATOR: ${operator.username}`);
        return;
    }

    sendMessage(ws, { event: 'auth_failed', message: 'Invalid username or password' });
    console.log(`❌ Client ${clientId} authentication failed`);
}

function handlePasswordChange(clientId, ws, msg) {
    const client = clients.get(clientId);
    if (client.role === 'admin') {
        sendMessage(ws, { event: 'password_changed' });
        console.log('🔐 Admin password changed');
    } else if (client.role === 'operator') {
        const op = operators.find(o => o.username === client.username);
        if (op && op.password === msg.oldPassword) {
            op.password = msg.newPassword;
            sendMessage(ws, { event: 'password_changed' });
            console.log(`🔐 Password changed for: ${client.username}`);
        } else {
            sendError(ws, 'Old password is incorrect');
        }
    }
}

// ==========================================================================
// Timer countdown loop (1 second tick)
// ==========================================================================

setInterval(() => {
    if (timerStatus !== 'RUNNING') return;

    if (mainTimerRemaining > 0) {
        mainTimerRemaining -= 1000;
        if (mainTimerRemaining < 0) mainTimerRemaining = 0;
    }

    // Event window hard cutoff
    if (activeEventEndTime > 0) {
        const nowSec = Math.floor(Date.now() / 1000);
        if (nowSec >= activeEventEndTime) {
            timerStatus = 'IDLE';
            mainTimerRemaining = 0;
            continuousMode = false;
            activeEventEndTime = 0;
            activeEventName = '';
            broadcast({ event: 'event_cutoff', message: 'Session ended - booking time expired' });
            broadcast({ event: 'reset' });
            console.log('⏰ Event window expired — hard cutoff');
            return;
        }
    }

    // Send sync
    broadcast(buildSyncMessage());

    // Check round finished
    if (mainTimerRemaining === 0) {
        if (continuousMode || currentRound < settings.numRounds) {
            if (pauseAfterNext) {
                currentRound++;
                mainTimerRemaining = settings.gameDuration;
                timerStatus = 'PAUSED';
                pauseAfterNext = false;
                broadcast({ event: 'pause' });
                broadcast(buildSyncMessage());
                console.log(`⏸️  Paused after round — now round ${currentRound}`);
            } else {
                currentRound++;
                mainTimerRemaining = settings.gameDuration;
                broadcast({
                    event: 'new_round',
                    status: 'RUNNING',
                    gameDuration: settings.gameDuration,
                    currentRound,
                    numRounds: settings.numRounds,
                    pauseAfterNext,
                    continuousMode,
                    activeEventEndTime,
                    activeEventName
                });
                console.log(`🔄 Starting round ${currentRound}`);
            }
        } else {
            timerStatus = 'FINISHED';
            continuousMode = false;
            activeEventEndTime = 0;
            activeEventName = '';
            broadcast({ event: 'finished' });
            console.log('⏰ All rounds finished!');
        }
    }
}, 1000);

// ==========================================================================
// NTP status broadcast
// ==========================================================================

setInterval(() => {
    broadcast({
        event: 'ntp_status',
        synced: true,
        time: getFormattedTime(),
        timezone: currentTimezone,
        autoSyncInterval: 30
    });
}, 5000);

// ==========================================================================
// Auto-trigger check (every 10 seconds)
// ==========================================================================

setInterval(() => {
    if (!hcEnabled) return;
    if (timerStatus !== 'IDLE' && timerStatus !== 'FINISHED') return;

    const nowSec = Math.floor(Date.now() / 1000);
    for (const ev of cachedEvents) {
        if (ev.triggered) continue;
        if (nowSec >= ev.startTime && nowSec <= ev.startTime + HC_TRIGGER_WINDOW_SEC) {
            ev.triggered = true;

            // Configure and start timer
            settings.gameDuration = ev.durationMin * 60 * 1000;
            timerStatus = 'RUNNING';
            mainTimerRemaining = settings.gameDuration;
            currentRound = 1;
            pauseAfterNext = false;

            if (ev.numRounds > 0) {
                settings.numRounds = ev.numRounds;
                continuousMode = false;
            } else {
                continuousMode = true;
            }

            activeEventEndTime = ev.endTime;
            activeEventName = ev.name;

            broadcast({ event: 'event_auto_started', eventName: ev.name, durationMin: ev.durationMin, eventEndTime: ev.endTime });
            broadcast({
                event: 'start',
                status: 'RUNNING',
                gameDuration: settings.gameDuration,
                currentRound,
                numRounds: settings.numRounds,
                pauseAfterNext,
                continuousMode,
                activeEventEndTime,
                activeEventName
            });
            broadcast({ event: 'upcoming_events', events: cachedEvents, lastSync: lastHCSync });
            console.log(`🚀 Auto-triggered: ${ev.name} (${ev.durationMin}min, ${ev.numRounds > 0 ? ev.numRounds + ' rounds' : 'continuous'})`);
            break;
        }
    }
}, 10000);

// ==========================================================================
// Hourly HC poll (+ purge expired)
// ==========================================================================

setInterval(() => {
    purgeExpiredEvents();
    if (hcEnabled && hcApiKey) {
        console.log('⏰ Hourly Hello Club poll...');
        fetchAndCacheEvents().then(success => {
            if (success) {
                broadcast({ event: 'upcoming_events', events: cachedEvents, lastSync: lastHCSync });
            }
        });
    }
}, 60 * 60 * 1000); // Every hour
