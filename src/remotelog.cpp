#include "remotelog.h"
#include <Arduino.h>

static LogEntry entries[RLOG_MAX_ENTRIES];
static int head = 0;       // Next write position
static int count = 0;      // Current number of entries
static uint32_t seq = 0;   // Global sequence counter

void remoteLogInit() {
    head = 0;
    count = 0;
    seq = 0;
    memset(entries, 0, sizeof(entries));
}

void remoteLog(const char* fmt, ...) {
    LogEntry& entry = entries[head];
    entry.timestamp = millis();

    va_list args;
    va_start(args, fmt);
    vsnprintf(entry.message, RLOG_MSG_LEN, fmt, args);
    va_end(args);

    // Also print to serial
    Serial.printf("[RLOG %lu] %s\n", entry.timestamp, entry.message);

    head = (head + 1) % RLOG_MAX_ENTRIES;
    if (count < RLOG_MAX_ENTRIES) count++;
    seq++;
}

String remoteLogGetAllJson() {
    // Pre-allocate to avoid repeated reallocation/fragmentation
    // Worst case: ~150 bytes per entry (JSON overhead + 100 char message)
    String json;
    json.reserve(50 + count * 150);
    json = "{\"event\":\"remote_log\",\"seq\":";
    json += String(seq);
    json += ",\"entries\":[";

    // Read from oldest to newest
    int start = (count < RLOG_MAX_ENTRIES) ? 0 : head;
    for (int i = 0; i < count; i++) {
        int idx = (start + i) % RLOG_MAX_ENTRIES;
        if (i > 0) json += ",";
        json += "{\"t\":";
        json += String(entries[idx].timestamp);
        json += ",\"m\":\"";
        // Escape quotes and backslashes in message
        for (const char* p = entries[idx].message; *p; p++) {
            if (*p == '"') json += "\\\"";
            else if (*p == '\\') json += "\\\\";
            else if (*p == '\n') json += "\\n";
            else json += *p;
        }
        json += "\"}";
    }

    json += "]}";
    return json;
}

uint32_t remoteLogGetSeq() {
    return seq;
}

int remoteLogCount() {
    return count;
}
