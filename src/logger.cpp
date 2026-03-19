#include "logger.hpp"
#include <cstdarg>
#include <ctime>
#include <iostream>
#include <stdio.h>

#include <windows.h>

static FILE *log_file_ptr = NULL;
static LogLevel current_log_level = LOG_LEVEL_INFO;
static const char *LOG_FILE_NAME = "RDR2BetterMouseSensitivity.log";

const char *log_level_to_string(LogLevel level)
{
    switch (level)
    {
    case LOG_LEVEL_DEBUG:
        return "DEBUG";
    case LOG_LEVEL_INFO:
        return "INFO";
    case LOG_LEVEL_WARNING:
        return "WARNING";
    case LOG_LEVEL_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

LogLevel string_to_log_level(const char *s)
{
    if (strcmp(s, "DEBUG") == 0)
        return LOG_LEVEL_DEBUG;
    if (strcmp(s, "INFO") == 0)
        return LOG_LEVEL_INFO;
    if (strcmp(s, "WARNING") == 0)
        return LOG_LEVEL_WARNING;
    if (strcmp(s, "ERROR") == 0)
        return LOG_LEVEL_ERROR;

    return LOG_LEVEL_UNKNOWN;
}

bool init_logger(LogLevel level)
{
    log_file_ptr = fopen(LOG_FILE_NAME, "w+");
    current_log_level = level;
    return log_file_ptr != NULL;
}

void shutdown_logger()
{
    if (log_file_ptr)
    {
        fclose(log_file_ptr);
        log_file_ptr = NULL;
    }
}

size_t get_time(char *Buffer, size_t BufferSize)
{
    std::time_t now = std::time(nullptr);
    std::tm ltm;
    localtime_s(&ltm, &now);
    return strftime(Buffer, BufferSize, "%d-%m-%Y %H:%M:%S", &ltm);
}

void log_message(LogLevel level, const char *Format, ...)
{
    char time_string_buffer[50];
    char formatted_message[256];
    char message_buffer[512];
    va_list args;

    if (level < current_log_level)
    {
        return;
    }

    // Parse format args.
    va_start(args, Format);
    vsnprintf(formatted_message, sizeof(formatted_message), Format, args);
    va_end(args);

    get_time(time_string_buffer, sizeof(time_string_buffer));
    snprintf(message_buffer, sizeof(message_buffer), "[%-7s][%s]: %s",
             log_level_to_string(level), time_string_buffer, formatted_message);
    OutputDebugStringA(message_buffer);
    if (log_file_ptr)
    {
        fprintf(log_file_ptr, "%s\n", message_buffer);
        fflush(log_file_ptr);
    }
}