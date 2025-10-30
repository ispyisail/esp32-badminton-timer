const express = require('express');
const WebSocket = require('ws');
const path = require('path');

const app = express();
const PORT = 8080;

// Serve static files from data directory
app.use(express.static(path.join(__dirname, '../data')));

const server = app.listen(PORT, () => {
    console.log(`\n🚀 Mock ESP32 Server running!`);
    console.log(`📱 Open your browser to: http://localhost:${PORT}`);
    console.log(`\n✨ Test credentials:`);
    console.log(`   Admin: username="admin", password="admin"`);
    console.log(`   Operator: username="operator1", password="pass123"`);
    console.log(`   Viewer: leave both fields empty\n`);
});

// WebSocket server
const wss = new WebSocket.Server({ server });

// Mock data storage
let timerRunning = false;
let mainTimerRemaining = 0;  // Changed from remainingTime (in milliseconds)
let breakTimerRemaining = 0;
let totalDuration = 120000;  // Changed to milliseconds (120 seconds = 120000ms)
let currentRound = 1;
let numRounds = 1;
let schedulingEnabled = false;
let schedules = [
    {
        id: "mock-1",
        clubName: "Test Club A",
        ownerUsername: "operator1",
        dayOfWeek: 1, // Monday
        startHour: 18,
        startMinute: 0,
        durationMinutes: 90,
        enabled: true,
        createdAt: Date.now()
    },
    {
        id: "mock-2",
        clubName: "Test Club B",
        ownerUsername: "operator1",
        dayOfWeek: 3, // Wednesday
        startHour: 19,
        startMinute: 30,
        durationMinutes: 60,
        enabled: true,
        createdAt: Date.now()
    }
];

let operators = [
    { username: "operator1", password: "pass123" },
    { username: "operator2", password: "test456" }
];

// Hello Club mock data
let helloClubSettings = {
    apiKey: '***configured***',
    enabled: false,
    daysAhead: 7,
    categoryFilter: '',
    syncHour: 0,
    lastSyncDay: -1
};

const mockHelloClubCategories = [
    'Badminton',
    'Tennis',
    'Pickleball',
    'Squash',
    'Table Tennis'
];

const mockHelloClubEvents = [
    {
        id: 'hc-event-1',
        name: 'Badminton Evening Session',
        startDate: getNextWeekday(1, 18, 0), // Next Monday 6 PM
        endDate: getNextWeekday(1, 19, 30), // 90 minutes
        activityName: 'Badminton',
        categoryName: 'Badminton',
        durationMinutes: 90
    },
    {
        id: 'hc-event-2',
        name: 'Competitive Badminton',
        startDate: getNextWeekday(3, 19, 0), // Next Wednesday 7 PM
        endDate: getNextWeekday(3, 20, 0), // 60 minutes
        activityName: 'Badminton',
        categoryName: 'Badminton',
        durationMinutes: 60
    },
    {
        id: 'hc-event-3',
        name: 'Casual Badminton',
        startDate: getNextWeekday(5, 18, 30), // Next Friday 6:30 PM
        endDate: getNextWeekday(5, 20, 0), // 90 minutes
        activityName: 'Badminton',
        categoryName: 'Badminton',
        durationMinutes: 90
    },
    {
        id: 'hc-event-4',
        name: 'Tennis Training',
        startDate: getNextWeekday(2, 17, 0), // Next Tuesday 5 PM
        endDate: getNextWeekday(2, 18, 30), // 90 minutes
        activityName: 'Tennis',
        categoryName: 'Tennis',
        durationMinutes: 90
    }
];

// Helper function to get next weekday at specific time
function getNextWeekday(targetDay, hour, minute) {
    const now = new Date();
    const result = new Date(now);
    const currentDay = now.getDay();
    let daysToAdd = targetDay - currentDay;
    if (daysToAdd <= 0) daysToAdd += 7;
    result.setDate(now.getDate() + daysToAdd);
    result.setHours(hour, minute, 0, 0);
    return result.toISOString();
}

// Client tracking
const clients = new Map();
let nextClientId = 1;

wss.on('connection', (ws) => {
    const clientId = nextClientId++;
    clients.set(clientId, {
        ws,
        role: 'viewer',
        username: 'Viewer'
    });

    console.log(`✅ Client ${clientId} connected`);

    // Send initial sync (matching ESP32 format)
    sendMessage(ws, {
        event: 'sync',
        mainTimerRemaining: mainTimerRemaining,
        breakTimerRemaining: breakTimerRemaining,
        serverMillis: Date.now(),
        currentRound: currentRound,
        numRounds: numRounds,
        status: timerRunning ? 'RUNNING' : 'PAUSED'
    });

    sendMessage(ws, {
        event: 'ntp_status',
        synced: true,
        time: new Date().toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: true }),
        timezone: 'Pacific/Auckland',
        dateTime: new Date().toISOString().replace('T', ' ').substring(0, 19),
        autoSyncInterval: 30
    });

    sendMessage(ws, {
        event: 'scheduling_status',
        enabled: schedulingEnabled
    });

    // Send initial settings
    sendMessage(ws, {
        event: 'settings',
        settings: {
            gameDuration: totalDuration,
            breakDuration: 60000, // 60 seconds default
            numRounds: numRounds,
            breakTimerEnabled: false,
            sirenLength: 5000,
            sirenPause: 1000
        }
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

function handleMessage(clientId, ws, msg) {
    const client = clients.get(clientId);
    console.log(`📨 Client ${clientId} [${client.username}]: ${msg.action}`);

    switch (msg.action) {
        case 'authenticate':
            handleAuth(clientId, ws, msg);
            break;

        case 'start':  // Changed from 'start_timer'
            if (client.role === 'viewer') {
                sendError(ws, 'Permission denied - viewer mode');
                return;
            }
            timerRunning = true;
            mainTimerRemaining = totalDuration;
            broadcast({
                event: 'sync',
                mainTimerRemaining: mainTimerRemaining,
                breakTimerRemaining: breakTimerRemaining,
                serverMillis: Date.now(),
                currentRound: currentRound,
                numRounds: numRounds,
                status: 'RUNNING'
            });
            console.log('⏱️  Timer started');
            break;

        case 'pause':  // Changed from 'stop_timer'
            if (client.role === 'viewer') {
                sendError(ws, 'Permission denied - viewer mode');
                return;
            }
            timerRunning = false;
            broadcast({
                event: 'sync',
                mainTimerRemaining: mainTimerRemaining,
                breakTimerRemaining: breakTimerRemaining,
                serverMillis: Date.now(),
                currentRound: currentRound,
                numRounds: numRounds,
                status: 'PAUSED'
            });
            console.log('⏸️  Timer paused');
            break;

        case 'reset':  // Added reset action
            if (client.role === 'viewer') {
                sendError(ws, 'Permission denied - viewer mode');
                return;
            }
            timerRunning = false;
            mainTimerRemaining = 0;
            breakTimerRemaining = 0;

            // Send reset event first (tells client to stop and clear display)
            broadcast({ event: 'reset' });

            // Then send sync with zeros
            broadcast({
                event: 'sync',
                mainTimerRemaining: 0,
                breakTimerRemaining: 0,
                serverMillis: Date.now(),
                currentRound: currentRound,
                numRounds: numRounds,
                status: 'PAUSED'
            });
            console.log('⏹️  Timer reset');
            break;

        case 'save_settings':  // Added save_settings
            if (client.role === 'viewer') {
                sendError(ws, 'Permission denied - viewer mode');
                return;
            }
            // Extract and apply settings
            if (msg.settings) {
                if (msg.settings.gameDuration) {
                    // gameDuration comes in milliseconds from client
                    totalDuration = msg.settings.gameDuration;
                    console.log(`⏲️  Game duration updated to ${totalDuration / 60000} minutes`);
                }
                if (msg.settings.numRounds) {
                    numRounds = msg.settings.numRounds;
                    console.log(`🔢 Number of rounds updated to ${numRounds}`);
                }
            }
            sendMessage(ws, { event: 'settings_saved' });

            // Send updated settings back to client
            sendMessage(ws, {
                event: 'settings',
                settings: {
                    gameDuration: totalDuration,
                    breakDuration: msg.settings?.breakDuration || 60000,
                    numRounds: numRounds,
                    breakTimerEnabled: msg.settings?.breakTimerEnabled || false,
                    sirenLength: msg.settings?.sirenLength || 5000,
                    sirenPause: msg.settings?.sirenPause || 1000
                }
            });
            console.log('💾 Settings saved');
            break;

        case 'set_duration':
            if (client.role === 'viewer') {
                sendError(ws, 'Permission denied - viewer mode');
                return;
            }
            totalDuration = msg.duration || 120;
            sendMessage(ws, { event: 'duration_set', duration: totalDuration });
            console.log(`⏲️  Duration set to ${totalDuration}s`);
            break;

        case 'enable_scheduling':  // Changed from 'set_scheduling'
            if (client.role === 'viewer') {
                sendError(ws, 'Permission denied - viewer mode');
                return;
            }
            schedulingEnabled = msg.enabled === true;
            broadcast({ event: 'scheduling_status', enabled: schedulingEnabled });
            console.log(`📅 Scheduling ${schedulingEnabled ? 'enabled' : 'disabled'}`);
            break;

        case 'get_schedules':
            const userSchedules = client.role === 'admin'
                ? schedules
                : schedules.filter(s => s.ownerUsername === client.username);
            sendMessage(ws, { event: 'schedules_list', schedules: userSchedules });
            break;

        case 'add_schedule':
            if (client.role === 'viewer') {
                sendError(ws, 'Permission denied - viewer mode');
                return;
            }
            const newSchedule = {
                ...msg.schedule,
                id: `mock-${Date.now()}`,
                ownerUsername: client.username,
                createdAt: Date.now()
            };
            schedules.push(newSchedule);
            sendMessage(ws, { event: 'schedule_added', schedule: newSchedule });
            console.log(`➕ Schedule added: ${newSchedule.clubName}`);
            break;

        case 'update_schedule':
            if (client.role === 'viewer') {
                sendError(ws, 'Permission denied - viewer mode');
                return;
            }
            const scheduleIndex = schedules.findIndex(s => s.id === msg.schedule.id);
            if (scheduleIndex === -1) {
                sendError(ws, 'Schedule not found');
                return;
            }
            const existing = schedules[scheduleIndex];
            if (client.role !== 'admin' && existing.ownerUsername !== client.username) {
                sendError(ws, 'Permission denied - you can only edit your own schedules');
                return;
            }
            schedules[scheduleIndex] = {
                ...msg.schedule,
                ownerUsername: existing.ownerUsername,
                createdAt: existing.createdAt
            };
            sendMessage(ws, { event: 'schedule_updated', schedule: schedules[scheduleIndex] });
            console.log(`✏️  Schedule updated: ${schedules[scheduleIndex].clubName}`);
            break;

        case 'delete_schedule':
            if (client.role === 'viewer') {
                sendError(ws, 'Permission denied - viewer mode');
                return;
            }
            const deleteIndex = schedules.findIndex(s => s.id === msg.id);
            if (deleteIndex === -1) {
                sendError(ws, 'Schedule not found');
                return;
            }
            const toDelete = schedules[deleteIndex];
            if (client.role !== 'admin' && toDelete.ownerUsername !== client.username) {
                sendError(ws, 'Permission denied - you can only delete your own schedules');
                return;
            }
            schedules.splice(deleteIndex, 1);
            sendMessage(ws, { event: 'schedule_deleted', id: msg.id });
            console.log(`🗑️  Schedule deleted: ${toDelete.clubName}`);
            break;

        case 'get_operators':
            if (client.role !== 'admin') {
                sendError(ws, 'Permission denied - admin only');
                return;
            }
            sendMessage(ws, { event: 'operators_list', operators: operators.map(o => ({ username: o.username })) });
            break;

        case 'add_operator':
            if (client.role !== 'admin') {
                sendError(ws, 'Permission denied - admin only');
                return;
            }
            operators.push({ username: msg.username, password: msg.password });
            sendMessage(ws, { event: 'operator_added', username: msg.username });
            console.log(`👤 Operator added: ${msg.username}`);
            break;

        case 'remove_operator':
            if (client.role !== 'admin') {
                sendError(ws, 'Permission denied - admin only');
                return;
            }
            operators = operators.filter(o => o.username !== msg.username);
            sendMessage(ws, { event: 'operator_removed', username: msg.username });
            console.log(`👤 Operator removed: ${msg.username}`);
            break;

        case 'change_password':
            handlePasswordChange(clientId, ws, msg);
            break;

        case 'factory_reset':
            if (client.role !== 'admin') {
                sendError(ws, 'Permission denied - admin only');
                return;
            }
            schedules = [];
            operators = [];
            schedulingEnabled = false;
            sendMessage(ws, { event: 'factory_reset_complete' });
            console.log('🔄 Factory reset completed');
            break;

        // Hello Club Integration
        case 'get_helloclub_settings':
            if (client.role !== 'admin') {
                sendError(ws, 'Permission denied - admin only');
                return;
            }
            sendMessage(ws, {
                event: 'helloclub_settings',
                ...helloClubSettings
            });
            console.log('⚙️  Hello Club settings requested');
            break;

        case 'save_helloclub_settings':
            if (client.role !== 'admin') {
                sendError(ws, 'Permission denied - admin only');
                return;
            }
            if (msg.apiKey && msg.apiKey !== '***configured***') {
                helloClubSettings.apiKey = '***configured***'; // Mask in test mode
            }
            if (msg.enabled !== undefined) helloClubSettings.enabled = msg.enabled;
            if (msg.daysAhead !== undefined) helloClubSettings.daysAhead = msg.daysAhead;
            if (msg.categoryFilter !== undefined) helloClubSettings.categoryFilter = msg.categoryFilter;
            if (msg.syncHour !== undefined) helloClubSettings.syncHour = msg.syncHour;

            sendMessage(ws, {
                event: 'helloclub_settings_saved',
                message: 'Hello Club settings saved successfully'
            });
            console.log('💾 Hello Club settings saved');
            break;

        case 'get_helloclub_categories':
            if (client.role !== 'admin') {
                sendError(ws, 'Permission denied - admin only');
                return;
            }
            sendMessage(ws, {
                event: 'helloclub_categories',
                categories: mockHelloClubCategories
            });
            console.log('📋 Hello Club categories requested');
            break;

        case 'get_helloclub_events':
            if (client.role !== 'admin') {
                sendError(ws, 'Permission denied - admin only');
                return;
            }

            // Filter events by category if filter is set
            let filteredEvents = mockHelloClubEvents;
            if (helloClubSettings.categoryFilter) {
                const filterCategories = helloClubSettings.categoryFilter.split(',').map(c => c.trim());
                filteredEvents = mockHelloClubEvents.filter(event =>
                    filterCategories.includes(event.categoryName)
                );
            }

            // Check for conflicts with existing schedules
            const eventsWithConflicts = filteredEvents.map(event => {
                const eventDate = new Date(event.startDate);
                const dayOfWeek = eventDate.getDay();
                const hour = eventDate.getHours();
                const minute = eventDate.getMinutes();

                const conflictingSchedule = schedules.find(s =>
                    s.dayOfWeek === dayOfWeek &&
                    s.startHour === hour &&
                    s.startMinute === minute
                );

                return {
                    ...event,
                    hasConflict: !!conflictingSchedule,
                    conflictWith: conflictingSchedule ? conflictingSchedule.clubName : undefined
                };
            });

            sendMessage(ws, {
                event: 'helloclub_events',
                events: eventsWithConflicts
            });
            console.log(`📅 Hello Club events requested (${eventsWithConflicts.length} events)`);
            break;

        case 'import_helloclub_events':
            if (client.role !== 'admin') {
                sendError(ws, 'Permission denied - admin only');
                return;
            }

            const eventIds = msg.eventIds || [];
            let imported = 0;
            let skipped = 0;

            eventIds.forEach(eventId => {
                const event = mockHelloClubEvents.find(e => e.id === eventId);
                if (!event) {
                    skipped++;
                    return;
                }

                const eventDate = new Date(event.startDate);
                const scheduleId = `hc-${eventId}`;

                // Check if already exists
                const existingIndex = schedules.findIndex(s => s.id === scheduleId);

                const newSchedule = {
                    id: scheduleId,
                    clubName: event.name,
                    ownerUsername: 'HelloClub',
                    dayOfWeek: eventDate.getDay(),
                    startHour: eventDate.getHours(),
                    startMinute: eventDate.getMinutes(),
                    durationMinutes: event.durationMinutes,
                    enabled: true,
                    createdAt: Date.now()
                };

                if (existingIndex >= 0) {
                    schedules[existingIndex] = newSchedule;
                } else {
                    schedules.push(newSchedule);
                }
                imported++;
                console.log(`📥 Imported: ${event.name}`);
            });

            sendMessage(ws, {
                event: 'helloclub_import_complete',
                imported,
                skipped
            });
            console.log(`✅ Hello Club import: ${imported} imported, ${skipped} skipped`);
            break;

        case 'sync_helloclub_now':
            if (client.role !== 'admin') {
                sendError(ws, 'Permission denied - admin only');
                return;
            }

            // Auto-import all matching events
            let autoImported = 0;
            let autoSkipped = 0;

            let eventsToSync = mockHelloClubEvents;
            if (helloClubSettings.categoryFilter) {
                const filterCategories = helloClubSettings.categoryFilter.split(',').map(c => c.trim());
                eventsToSync = mockHelloClubEvents.filter(event =>
                    filterCategories.includes(event.categoryName)
                );
            }

            eventsToSync.forEach(event => {
                const eventDate = new Date(event.startDate);
                const dayOfWeek = eventDate.getDay();
                const hour = eventDate.getHours();
                const minute = eventDate.getMinutes();

                // Check for conflicts
                const hasConflict = schedules.some(s =>
                    s.dayOfWeek === dayOfWeek &&
                    s.startHour === hour &&
                    s.startMinute === minute &&
                    !s.id.startsWith('hc-')
                );

                if (hasConflict) {
                    autoSkipped++;
                    return;
                }

                const scheduleId = `hc-${event.id}`;
                const existingIndex = schedules.findIndex(s => s.id === scheduleId);

                const newSchedule = {
                    id: scheduleId,
                    clubName: event.name,
                    ownerUsername: 'HelloClub',
                    dayOfWeek: dayOfWeek,
                    startHour: hour,
                    startMinute: minute,
                    durationMinutes: event.durationMinutes,
                    enabled: true,
                    createdAt: Date.now()
                };

                if (existingIndex >= 0) {
                    schedules[existingIndex] = newSchedule;
                } else {
                    schedules.push(newSchedule);
                }
                autoImported++;
            });

            helloClubSettings.lastSyncDay = new Date().getDay();

            sendMessage(ws, {
                event: 'helloclub_sync_complete',
                message: `Sync complete: ${autoImported} imported, ${autoSkipped} skipped`
            });
            console.log(`🔄 Hello Club sync: ${autoImported} imported, ${autoSkipped} skipped`);
            break;

        default:
            console.log(`⚠️  Unknown action: ${msg.action}`);
    }
}

function handleAuth(clientId, ws, msg) {
    const client = clients.get(clientId);

    // Viewer mode (empty credentials)
    if (!msg.username && !msg.password) {
        client.role = 'viewer';
        client.username = 'Viewer';
        sendMessage(ws, {
            event: 'viewer_mode',
            role: 'viewer',
            username: 'Viewer'
        });
        console.log(`👁️  Client ${clientId} viewing as guest`);
        return;
    }

    // Admin check
    if (msg.username === 'admin' && msg.password === 'admin') {
        client.role = 'admin';
        client.username = 'admin';
        sendMessage(ws, {
            event: 'auth_success',
            role: 'admin',
            username: 'admin'
        });
        console.log(`🔑 Client ${clientId} logged in as ADMIN`);
        return;
    }

    // Operator check
    const operator = operators.find(o => o.username === msg.username && o.password === msg.password);
    if (operator) {
        client.role = 'operator';
        client.username = operator.username;
        sendMessage(ws, {
            event: 'auth_success',
            role: 'operator',
            username: operator.username
        });
        console.log(`🔑 Client ${clientId} logged in as OPERATOR: ${operator.username}`);
        return;
    }

    // Failed auth
    sendError(ws, 'ERR_AUTH_FAILED: Invalid username or password');
    console.log(`❌ Client ${clientId} authentication failed`);
}

function handlePasswordChange(clientId, ws, msg) {
    const client = clients.get(clientId);

    if (client.role === 'admin') {
        sendMessage(ws, { event: 'password_changed' });
        console.log('🔐 Admin password changed');
    } else if (client.role === 'operator') {
        const operator = operators.find(o => o.username === client.username);
        if (operator && operator.password === msg.oldPassword) {
            operator.password = msg.newPassword;
            sendMessage(ws, { event: 'password_changed' });
            console.log(`🔐 Password changed for: ${client.username}`);
        } else {
            sendError(ws, 'ERR_PASSWORD_CHANGE: Old password is incorrect');
        }
    }
}

function sendMessage(ws, data) {
    if (ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify(data));
    }
}

function sendError(ws, message) {
    sendMessage(ws, { event: 'error', message });
}

function broadcast(data) {
    clients.forEach(client => {
        sendMessage(client.ws, data);
    });
}

// Simulate timer countdown (update every 100ms for smooth countdown)
setInterval(() => {
    if (timerRunning && mainTimerRemaining > 0) {
        mainTimerRemaining -= 100;  // Decrease by 100ms
        if (mainTimerRemaining < 0) mainTimerRemaining = 0;

        // Broadcast sync every 100ms
        broadcast({
            event: 'sync',
            mainTimerRemaining: mainTimerRemaining,
            breakTimerRemaining: breakTimerRemaining,
            serverMillis: Date.now(),
            currentRound: currentRound,
            numRounds: numRounds,
            status: 'RUNNING'
        });

        if (mainTimerRemaining === 0) {
            timerRunning = false;
            console.log('⏰ Timer finished!');
        }
    }
}, 100);  // Update every 100ms

// Simulate NTP status updates
setInterval(() => {
    broadcast({
        event: 'ntp_status',
        synced: true,
        time: new Date().toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: true }),
        timezone: 'Pacific/Auckland',
        dateTime: new Date().toISOString().replace('T', ' ').substring(0, 19),
        autoSyncInterval: 30
    });
}, 5000);
