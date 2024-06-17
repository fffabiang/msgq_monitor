#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

typedef enum {
    LOG_DEBUG,    // D
    LOG_INFO,     // I
    LOG_NOTICE,   // N
    LOG_ERROR     // E
} LogLevel;


// Global variables
extern char logfile_name[100];  // Log file name
extern char log_type;          // Field 1: Type of log
extern LogLevel min_log_level;

// Log level macros
#define DEB "D"
#define NOT "N"
#define INF "I"
#define WAR "W"
#define ERR "E"
#define AUD "A"

// Functions 

// Macro to wrap the log_message function
#define LOG(level, format, ...) log_message(level, __FILE__, __func__, __LINE__, format, ##__VA_ARGS__)

void log_message(const char *level, const char *file, const char *func, int line, const char *format, ...);

char get_min_log_level_char();

#endif // LOGGER_H