#include "logger.h"
#include <sys/time.h>

/*
The following format is considered for this LOG (Standard Log v03)
    Field 1 (1 byte) Type of log. Allowed values: E (events), T (trazado), P (performance), B (errors), X (transactions), A (audit), S(security) , U (undefined)
    White space
    Field 2 (2 bytes): Format version. Fixed value "03"
    White space
    Field 3 (1 byte): Level. Allowed values: D (debug), I (informative), N(notification), W (warning), E(error), A (audit)
    White space
    Field 4 (28 bytes). Date/time in the format DDD MMM dd hh.:mm
    .mmm YYYY where DDD = Mon, Tue, Wed, Thu, Fri, Sat, Sun and MMM = Jan, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec
    White space
    Field 5 (16 bytes) Module name. If higher than 16 chars only print the first 16, pad with whitespaces to the left to fill 16 bytes if needed.
    Whitespace
    Field 6 (8 bytes) Module version. Example (01.00.01). Pad left if necessary.
    White space
    Filler (10 bytes). Future use field. Fill it with 10 whitespaces. (NOT USED RIGHT NOW)
    White space
    Field 7 (22 bytes). Thread identification number. Right padding with whitespaces.
    White space
    Field 8 (12 bytes) Node ID value. Left padding with whitespaces.
    Whitespace
    Field 9 (32 bytes) Message ID. If does not apply, pad with '0'
    Whitespace
    Field 10 (80 bytes) Source file name. If higher than 80, only use the last 80 chars. Left padding with whitespaces.
    Whitespace
    Field 11 (20 bytes): Function called namne. If higher than 20 chars, use the last 20. Left padding with whitespaces.
    Whitespace
    Field 12 (5 bytes): source code line, left padding.
    Whitespace
    Field 13 (variable).. Message string
*/

// Global variables
char logfile_name[50] = "logfile.log"; // Log file name
char log_type = 'E';                   // Field 1: Type of log
LogLevel min_log_level = LOG_DEBUG;

static char format_version[3] = "03";    // Field 2: Format version
char module_name[17] = "sixbasbo"; // Field 5: Module name
char module_version[9] = "01.00.00"; // Field 6: Module version
static int module_name_loaded = 0;
static char node_id[13] = "000000000000"; // Field 8: Node ID value
static char tx_message_id[33] = "00000000000000000000000000000000"; // Field 9: Message ID

// Function to retrieve the current process name
void get_process_name(char *process_name, size_t size) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", getpid());
    FILE *f = fopen(path, "r");
    if (f) {
        fgets(process_name, size, f);
        fclose(f);
    } else {
        strncpy(process_name, "unknown", size);
    }

    // Ensure null termination and remove any path components
    process_name[size - 1] = '\0';
    char *last_slash = strrchr(process_name, '/');
    if (last_slash) {
        memmove(process_name, last_slash + 1, strlen(last_slash));
    }
}

char get_min_log_level_char()
{
    switch(min_log_level)
    {
        case LOG_DEBUG:
            return 'D';
        case LOG_INFO:
            return 'I';
        case LOG_NOTICE:
            return 'N';
        case LOG_ERROR:
            return 'E';
        default:
            return 'D';
    }
}

int log_level_allowed(const char* level)
{

    LogLevel logLevel;
    if (strcmp(level, DEB) == 0) {
        logLevel = LOG_DEBUG;
    } else if (strcmp(level, INF) == 0) {
        logLevel = LOG_INFO;
    } else if (strcmp(level, NOT) == 0 || strcmp(level, WAR) == 0 || strcmp(level, AUD) == 0) {
        logLevel = LOG_NOTICE;
    } else {
        logLevel = LOG_ERROR;
    }

    // Determine if the log level is allowed
    switch (min_log_level) {
        case LOG_DEBUG:
            // Allow all levels
            return 1;
        case LOG_INFO:
            // Allow all levels except DEBUG
            return logLevel != LOG_DEBUG;
        case LOG_NOTICE:
            // Allow all levels except DEBUG, INFO, and AUDIT
            return logLevel != LOG_DEBUG && logLevel != LOG_INFO;
        case LOG_ERROR:
            // Allow only ERROR level
            return logLevel == LOG_ERROR;
        default:
            // If min_log_level is set to an unknown value, deny all
            return 0;
    }

}


void log_message(const char *level, const char *file, const char *func, int line, const char *format, ...) {

    if (!log_level_allowed(level))
    {
        return;
    }
    
    if (!module_name_loaded) {
        // Retrieve the current process name
        get_process_name(module_name, sizeof(module_name));
        module_name_loaded = 1;
    }

    // Open the log file
    FILE *log_file = fopen(logfile_name, "a");
    if (!log_file) {
        perror("Failed to open log file");
        return;
    }

    // Retrieve current date and time
    // char datetime[29];
    // time_t t = time(NULL);
    // struct tm *tm_info = localtime(&t);
    // strftime(datetime, 29, "%a %b %d %H:%M:%S.000 %Y", tm_info);

    // Retrieve current date and time with milliseconds
    char datetime[40];
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *tm_info = localtime(&tv.tv_sec);
    strftime(datetime, sizeof(datetime), "%a %b %d %H:%M:%S", tm_info);

    // Append milliseconds and year
    char datetime_with_ms[70]; // Increase buffer size to be safe
    snprintf(datetime_with_ms, sizeof(datetime_with_ms), "%s.%03ld %d", datetime, tv.tv_usec / 1000, tm_info->tm_year + 1900);


    // Prepare the message
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    // Format the log line
    char log_line[1500];
    snprintf(log_line, sizeof(log_line), 
             "%c %s %s %s %16.16s %-8.8s %-22d %12.12s %-32.32s %30.30s %20.20s %5d %s\n",
             log_type,
             format_version,
             level,
             datetime_with_ms,
             module_name,
             module_version,
             getpid(),
             node_id,
             tx_message_id,
             file,
             func,
             line,
             message);

    // Write the log line to the file
    fputs(log_line, log_file);

    // Close the log file
    fclose(log_file);
}