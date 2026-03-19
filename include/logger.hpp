#pragma once

#define LOG_DEBUG(...) log_message(LOG_LEVEL_DEBUG, __VA_ARGS__)
#define LOG_INFO(...) log_message(LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_WARNING(...) log_message(LOG_LEVEL_WARNING, __VA_ARGS__)
#define LOG_ERROR(...) log_message(LOG_LEVEL_ERROR, __VA_ARGS__)

enum LogLevel
{
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_UNKNOWN
};

const char *log_level_to_string(enum LogLevel level);
LogLevel string_to_log_level(const char *s);

bool init_logger(LogLevel Level);
void shutdown_logger();
void log_message(LogLevel Level, const char *Format, ...);