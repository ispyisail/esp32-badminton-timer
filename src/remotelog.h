#pragma once

#include <Arduino.h>

// =============================================================================
// Remote Diagnostic Log — RAM ring buffer viewable via WebSocket
// =============================================================================

constexpr int RLOG_MAX_ENTRIES = 40;
constexpr int RLOG_MSG_LEN = 100;

struct LogEntry {
    unsigned long timestamp;      // millis()
    char message[RLOG_MSG_LEN];
};

void remoteLogInit();
void remoteLog(const char* fmt, ...);
String remoteLogGetAllJson();
uint32_t remoteLogGetSeq();
int remoteLogCount();
