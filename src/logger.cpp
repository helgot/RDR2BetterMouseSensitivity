#include "logger.hpp"
#include <cstdarg>
#include <ctime>
#include <iostream>
#include <stdio.h>

#include <windows.h>

static FILE *LogFilePtr = NULL;
static LogLevel CurrentLogLevel = LOG_LEVEL_INFO;
static const char *kLogFileName = "RDR2BetterMouseSensitivity.log";

static const char *LogLevelStrings[] = {"DEBUG", "INFO", "WARNING", "ERROR"};

const char *LogLevelToString(LogLevel Level)
{
    switch (Level)
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

LogLevel StringToLogLevel(const char *s)
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

bool InitLogger(LogLevel Level)
{
    LogFilePtr = fopen(kLogFileName, "w+");
    CurrentLogLevel = Level;
    return LogFilePtr != NULL;
}

void ShutdownLogger()
{
    if (LogFilePtr)
    {
        fclose(LogFilePtr);
        LogFilePtr = NULL;
    }
}

size_t GetTime(char *Buffer, size_t BufferSize)
{
    std::time_t now = std::time(nullptr);
    std::tm ltm;
    localtime_s(&ltm, &now);
    return strftime(Buffer, BufferSize, "%d-%m-%Y %H:%M:%S", &ltm);
}

void LogMessage(LogLevel Level, const char *Format, ...)
{
    char TimeBuffer[50];
    char FormattedMessage[256];
    char MessageBuffer[512];
    va_list Args;

    if (Level < CurrentLogLevel)
    {
        return;
    }

    // Parse format args.
    va_start(Args, Format);
    vsnprintf(FormattedMessage, sizeof(FormattedMessage), Format, Args);
    va_end(Args);

    GetTime(TimeBuffer, sizeof(TimeBuffer));
    snprintf(MessageBuffer, sizeof(MessageBuffer), "[%-7s][%s]: %s",
             LogLevelToString(Level), TimeBuffer, FormattedMessage);
    OutputDebugStringA(MessageBuffer);
    if (LogFilePtr)
    {
        fprintf(LogFilePtr, "%s\n", MessageBuffer);
        fflush(LogFilePtr);
    }
}