#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ctype.h>
#include <stdarg.h>
#include "logger.h"

#define BUFFER_SIZE 1024
#define MAX_LIST_ITEMS 100
#define MAX_KEY_LENGTH 20 // Including null terminator
#define MAX_MAIL_MESSAGE_LENGTH 5000

#define MAX_CONFIG_PARAM_KEY 50
#define MAX_CONFIG_PARAM_VALUE 450
#define STRINGIFY(x) STRINGIFY2(x)
#define STRINGIFY2(x) #x
#define MAX_CONFIG_LINE_SIZE (MAX_CONFIG_PARAM_KEY + MAX_CONFIG_PARAM_VALUE)

char paramMailRecipients[MAX_CONFIG_PARAM_VALUE];
char paramMailContent[MAX_CONFIG_PARAM_VALUE];
char paramMailSubject[MAX_CONFIG_PARAM_VALUE];

// External Log variables
extern char logfile_name[50];
extern LogLevel min_log_level;


void read_list_from_config(const char *filename, char list[MAX_LIST_ITEMS][MAX_KEY_LENGTH], int *list_count) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    char line[256];
    int in_list_section = 0;
    *list_count = 0;

    while (fgets(line, sizeof(line), file)) {
        // Trim newline characters
        line[strcspn(line, "\r\n")] = '\0';

        // Check for the beginning of the list section
        if (strcmp(line, "LIST BEGIN") == 0) {
            in_list_section = 1;
            continue;
        }

        // Check for the end of the list section
        if (strcmp(line, "LIST END") == 0) {
            in_list_section = 0;
            continue;
        }

        // If we're in the list section, add the item to the array
        if (in_list_section) {
            if (*list_count >= MAX_LIST_ITEMS) {
                LOG(ERR, "Error: Too many items in the list");
                exit(EXIT_FAILURE);
            }
            // Ensure the string length is within bounds before copying
            if (strlen(line) >= MAX_KEY_LENGTH) {
                LOG(ERR, "Error: List item too long");
                exit(EXIT_FAILURE);
            }
            strncpy(list[*list_count], line, MAX_KEY_LENGTH - 1);
            list[*list_count][MAX_KEY_LENGTH - 1] = '\0'; // Ensure null-termination
            (*list_count)++;
        }
    }

    fclose(file);
}



void get_msgqueue_usage(const char* key, int* used_bytes, int* messages) {
    FILE *fp;
    char buffer[BUFFER_SIZE];
    char command[] = "ipcs -q";
    char *line;
    int found = 0;

    // Execute the ipcs -q command and open a pipe to read the output
    if ((fp = popen(command, "r")) == NULL) {
        LOG(ERR,"popen");
        exit(EXIT_FAILURE);
    }

    // Read the command output line by line
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        // Check if the line contains the key
        if (strstr(buffer, key) != NULL) {
            found = 1;
            break;
        }
    }

    // Close the pipe
    if (pclose(fp) == -1) {
        perror("pclose");
        exit(EXIT_FAILURE);
    }

    if (!found) {
        LOG(ERR, "No message queue with key %s found.\n", key);
        exit(EXIT_FAILURE);
    }

    // Tokenize the line to extract the used-bytes and messages values
    // Sample line format: "0x00000062 123456789  username  644  0  128  10"

    line = strtok(buffer, " \t\n");
    int column = 1;
    *used_bytes = -1;
    *messages = -1;

    while (line != NULL) {
        if (column == 5) {
            *used_bytes = atoi(line);
        } else if (column == 6) {
            *messages = atoi(line);
        }

        line = strtok(NULL, " \t\n");
        column++;
    }
}

int isKeyInKeyList(char* key, char key_list[MAX_LIST_ITEMS][MAX_KEY_LENGTH], int key_list_count)
{
    for (int i = 0; i < key_list_count; i++) {
        if (strcmp(key, key_list[i]) == 0) {
            return 1; // Key found
        }
    }
    return 0; // Key not found
}

int is_queue_over_limit(int queue_usage, int max_queue_bytes, int usage_limit)
{
    return  (queue_usage * 100 / max_queue_bytes) >= usage_limit;
}


void build_alert_message(char alert_key_list[MAX_LIST_ITEMS][MAX_KEY_LENGTH], int alert_key_count, int usage_limit, char *message) {
    
    // Initialize the message
    snprintf(message, MAX_MAIL_MESSAGE_LENGTH, "%s\nSIX server alert: There were %d queues found over the %d%% limit.\n",paramMailContent, alert_key_count, usage_limit);
    strcat(message, "Queue keys to check:\n");

    // Add each queue key to the message
    for (int i = 0; i < alert_key_count; i++) {
        strcat(message, "\t- ");
        strcat(message, alert_key_list[i]);
        strcat(message, "\n");
    }

    // Append the final line
    strcat(message, "Please review the server.\n");
}


void send_mailx(char alert_key_list[MAX_LIST_ITEMS][MAX_KEY_LENGTH], int alert_key_count, int usage_limit)
{
    int status; // to receive mailx system call exit code
    char command[MAX_MAIL_MESSAGE_LENGTH + 256];
    char mailRecipients[256];

    // Replace commas with spaces in the paramMailRecipients string
    strcpy(mailRecipients, paramMailRecipients);
    for (char* p = mailRecipients; *p != '\0'; ++p) {
        if (*p == ',') {
            *p = ' ';
        }
    }

    char mailContent[MAX_MAIL_MESSAGE_LENGTH];
    build_alert_message(alert_key_list, alert_key_count, usage_limit, mailContent);

    snprintf(command, sizeof(command), "echo '%s' | mailx -s '%s' %s", mailContent, paramMailSubject, mailRecipients);

    LOG(INF, "Executing mail command with (%d) bytes", strlen(command));

    // Execute the command
    status = system(command);

    // The command was executed; check the exit status
    if (WIFEXITED(status)) {
        int exitStatus = WEXITSTATUS(status);
        LOG(NOT, "Notification mail: mailx exited with status (%d)", exitStatus);

    } else {
        LOG(ERR, "Notification mail: mailx exited abnormally");
    }


}

// Function to print message queue usage based on mode and owner
void process_msgqueue_usage(char mode, const char *owner, char key_list[MAX_LIST_ITEMS][MAX_KEY_LENGTH], int key_list_count, int max_queue_bytes, int usage_limit) {
    
    FILE *fp;
    char buffer[BUFFER_SIZE];
    char command[] = "ipcs -q";
    char *line;
    int alert_key_count = 0;
    char alert_key_list[MAX_LIST_ITEMS][MAX_KEY_LENGTH];
    int queues_processed = 0;
    // Execute the ipcs -q command and open a pipe to read the output
    if ((fp = popen(command, "r")) == NULL) {
        LOG(ERR, "Error: Unable to execute command 'ipcs -q'");
        exit(EXIT_FAILURE);
    }


    // Read the command output line by line
    LOG(DEB,"Processing ipcs message queue usage");
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {

        // Check if the line contains column headers
        if (strstr(buffer, "Message") != NULL || strstr(buffer, "key") != NULL || strstr(buffer, "msqid") != NULL ||
            strstr(buffer, "owner") != NULL || strstr(buffer, "perms") != NULL ||
            strstr(buffer, "used-bytes") != NULL || strstr(buffer, "messages") != NULL) {
            // Skip the header lines
            continue;
        }

        // Tokenize the line to extract relevant information
        char *key, *queue_owner;
        
        char *token = strtok(buffer, " \t\n");        
        if (token != NULL) { 
            key = token;  // Token 1: key
            token = strtok(NULL, " \t\n");  if (token == NULL) continue; // Token 2: msgqid
            token = strtok(NULL, " \t\n");  if (token == NULL) continue; // Token 3: owner
            queue_owner = token;            
            token = strtok(NULL, " \t\n");  if (token == NULL) continue; // Token 4: perms
            token = strtok(NULL, " \t\n");  if (token == NULL) continue; // Token 5: used_bytes
            int bytes = atoi(token);
            token = strtok(NULL, " \t\n");  // Token 6: messages
            int messages = atoi(token);
            if ( mode == 'A' || (mode == 'O' && strcmp(queue_owner, owner) == 0) || (mode == 'L' && isKeyInKeyList(key, key_list, key_list_count))) {
                LOG(INF,"Message Queue Key: %s, Owner: %s, Used Bytes: %d, Messages: %d",
                                    queue_owner, key, bytes, messages);
                
                if (is_queue_over_limit(bytes, max_queue_bytes, usage_limit))
                {
                    LOG(NOT,"Message Queue %s over %d%c limit", key, usage_limit, '%'); 
                    strcpy(alert_key_list[alert_key_count],key);
                    alert_key_count++;
                }
                queues_processed++;
            }
        }
    }


    // Close the pipe
    LOG(DEB,"Finished processing ipcs command output");
    if (pclose(fp) == -1) {
        perror("pclose");
        exit(EXIT_FAILURE);
    }

    LOG(INF,"Processed %d system queues.", queues_processed);

    // Send alert 
    if (alert_key_count > 0)
    {
        LOG(NOT,"Found %d queues over %d%c limit. Sending mail notification.", alert_key_count, usage_limit, '%');
        send_mailx(alert_key_list, alert_key_count, usage_limit);
    }else{
        LOG(INF,"No queues over limit");
    }
}

int get_value_from_file(const char *filename) {
    FILE *fp;
    int value;

    fp = fopen(filename, "r");
    if (fp == NULL) {
        LOG(ERR,"fopen");
        exit(EXIT_FAILURE);
    }

    if (fscanf(fp, "%d", &value) != 1) {
        LOG(ERR,"fscanf");
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    fclose(fp);
    return value;
}

void get_msg_queue_limits(int *msgmax, int *msgmnb, int *msgmni) {
    // Read values from /proc/sys/kernel/msgmax
    *msgmax = get_value_from_file("/proc/sys/kernel/msgmax");

    // Read values from /proc/sys/kernel/msgmnb
    *msgmnb = get_value_from_file("/proc/sys/kernel/msgmnb");

    // Read values from /proc/sys/kernel/msgmni
    *msgmni = get_value_from_file("/proc/sys/kernel/msgmni");
}



// Function to trim leading and trailing whitespace
char *trim_whitespace(char *str) {
    char *end;

    // Trim leading space
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) // All spaces
        return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator
    end[1] = '\0';

    return str;
}

// Function to trim leading and trailing whitespace and newline characters
void trim(char *str) {
    char *start = str;
    char *end;

    // Trim leading space
    while (isspace((unsigned char)*start)) start++;

    if (*start == 0) {  // All spaces?
        *str = '\0';
        return;
    }

    // Trim trailing space
    end = start + strlen(start) - 1;
    while (end > start && (isspace((unsigned char)*end) || *end == '\n')) end--;

    // Write new null terminator
    *(end + 1) = '\0';

    // Shift the trimmed string back to the beginning
    if (start != str) {
        memmove(str, start, end - start + 2); // +2 to include the null terminator
    }
}

void read_config(const char *filename, char *mode, char *owner, int* usage_limit) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        LOG(ERR,"Error opening file %s", filename);
        exit(EXIT_FAILURE);
    }

    char line[MAX_CONFIG_LINE_SIZE];
    while (fgets(line, sizeof(line), file)) {
                
        char key[MAX_CONFIG_PARAM_KEY], value[MAX_CONFIG_PARAM_VALUE];

        // Ignore commented lines
        if (line[0] == '#' || line[0]=='\n') continue;
        
        if (sscanf(line, "%" STRINGIFY(MAX_CONFIG_PARAM_KEY) "[^=]=%" STRINGIFY(MAX_CONFIG_PARAM_VALUE) "[^\n]", key, value) == 2) {            
            
            trim(key);
            trim(value);

            //LOG(DEB,"key (%s) value (%s)\n", key, value);
            
            if (strcmp(key, "MODE") == 0) {
                *mode = value[0];  // Store the first character of the value
            } else if (strcmp(key, "OWNER") == 0) {
                strcpy(owner, value);
            } else if (strcmp(key, "LIMIT") == 0)
            {
                *usage_limit = atoi(value);
            } else if (strcmp(key, "mail_subject") == 0) {
                strcpy(paramMailSubject, value);
            } else if (strcmp(key, "mail_recipients") == 0) {
                strcpy(paramMailRecipients, value);
            } else if (strcmp(key, "mail_content_header") == 0) {
                strcpy(paramMailContent, value);
            } else if (strcmp(key, "LOG_LEVEL") == 0)
            {
                // Set min log value
                switch (value[0])
                {
                    case 'D':
                        min_log_level = LOG_DEBUG;
                        break;
                    case 'I':
                        min_log_level = LOG_INFO;
                        break;
                    case 'N':
                        min_log_level = LOG_NOTICE;
                        break;
                    case 'E':
                        min_log_level = LOG_ERROR;
                        break;
                    default:
                        break;
                }                
            }
        }
    }

    fclose(file);
}

int main(int argc, char *argv[]) {
    
    char mode = '\0';
    char owner[MAX_CONFIG_PARAM_VALUE] = "";
    int usage_limit = -1;
    int msgmax, msgmnb, msgmni, max_messages;
    int used_bytes, messages;
    char key_list[MAX_LIST_ITEMS][MAX_KEY_LENGTH];
    int key_list_count;

    // Set custom LOG filename
    strcpy(logfile_name, "monmsgq-total.log");
    
    LOG(NOT, "Monitor process starts.");

    // Validate number of arguments
    if (argc != 2) {
        LOG(ERR, "Error: usage: %s <config_filename>", argv[0]);
        exit(EXIT_FAILURE);
    }
 
    // Validate config file
    read_config(argv[1], &mode, owner, &usage_limit);

    LOG(INF,"Execution parameters | Mode: %c | Owner: %s | Limit: %d%% | Min Log Level %c", mode, owner, usage_limit, get_min_log_level_char());
    
    if (strlen(paramMailRecipients) == 0 || strlen(paramMailSubject) == 0 || strlen(paramMailContent) == 0) {
        LOG(ERR, "Missing one or more valid mail parameters in configuration file.");
        exit(EXIT_FAILURE);
    }else if (mode == '\0')
    {
        LOG(ERR, "Missing MODE parameter in configuration file.");
        exit(EXIT_FAILURE);
    }else if (usage_limit < 0 || usage_limit > 99){
        LOG(ERR, "Missing valid LIMIT (0-99) parameter in configuration file.");
        exit(EXIT_FAILURE);
    }else if (mode == 'O' && strlen(owner)==0)
    {
        LOG(ERR, "Missing OWNER parameter in configuration file.");
        exit(EXIT_FAILURE);
    }
    
    // Read list keys in mode L
    if (mode == 'L')
    {
        read_list_from_config(argv[1], key_list, &key_list_count);
    }   

    // Get message queue limits
    get_msg_queue_limits(&msgmax, &msgmnb, &msgmni);

    // Print the values
    LOG(DEB,"OS message queue limits | max msg size (%d) | max queue bytes (%d)", msgmax, msgmnb);

    // Print message queue search
    process_msgqueue_usage(mode, owner, key_list, key_list_count, msgmnb, usage_limit);

    LOG(NOT, "Monitor process finished.");

    return 0;
}

