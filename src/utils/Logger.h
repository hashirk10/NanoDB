#pragma once
#include <cstdio>
#include <ctime>
#include <cstring>
#include <cstdarg>

class Logger {
    static FILE* logFile;
    static bool  initialized;

    static void init() {
        if (!initialized) {
            logFile     = fopen("nanodb_execution.log", "w");
            initialized = true;
        }
    }

public:
    // Set to true during bulk data load to suppress LRU eviction spam
    static bool silent;

    static void log(const char* fmt, ...) {
        init();
        char msg[4096];
        va_list args;
        va_start(args, fmt);
        vsnprintf(msg, sizeof(msg), fmt, args);
        va_end(args);

        // Suppress "Page N evicted" messages during silent (bulk-load) phase
        if (silent && strncmp(msg, "Page ", 5) == 0) return;

        time_t now = time(nullptr);
        struct tm* ti = localtime(&now);
        char ts[32];
        strftime(ts, sizeof(ts), "%H:%M:%S", ti);

        if (logFile) {
            fprintf(logFile, "[%s] [LOG] %s\n", ts, msg);
            fflush(logFile);
        }
        printf("[LOG] %s\n", msg);
    }

    static void setSilent(bool s) { silent = s; }

    static void close() {
        if (logFile) { fclose(logFile); logFile = nullptr; initialized = false; }
    }
};

inline FILE* Logger::logFile     = nullptr;
inline bool  Logger::initialized = false;
inline bool  Logger::silent      = false;
