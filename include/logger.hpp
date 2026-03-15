#pragma once

#define LOG_DEBUG(...) LogMessage(LOG_LEVEL_DEBUG, __VA_ARGS__)
#define LOG_INFO(...) LogMessage(LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_WARNING(...) LogMessage(LOG_LEVEL_WARNING, __VA_ARGS__)
#define LOG_ERROR(...) LogMessage(LOG_LEVEL_ERROR, __VA_ARGS__)

enum LogLevel
{
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_UNKNOWN
};

const char *LogLevelToString(enum LogLevel level);
LogLevel StringToLogLevel(const char *s);

bool InitLogger(LogLevel Level);
void ShutdownLogger();
void LogMessage(LogLevel Level, const char *Format, ...);