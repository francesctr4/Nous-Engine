#ifndef NOUS_ENGINE_LOGGERBINDINGS_H
#define NOUS_ENGINE_LOGGERBINDINGS_H

// --- Subsystem APIs ---
struct LoggerAPI
{
    // Function pointers exposed to scripts
    void (*Trace)(const char* msg, ...) = nullptr;
    void (*Debug)(const char* msg, ...) = nullptr;
    void (*Info)(const char* msg, ...) = nullptr;
    void (*Warn)(const char* msg, ...) = nullptr;
    void (*Error)(const char* msg, ...) = nullptr;
    void (*Fatal)(const char* msg, ...) = nullptr;
};

void SetupLoggerBindings(LoggerAPI& logger);

#endif //NOUS_ENGINE_LOGGERBINDINGS_H
