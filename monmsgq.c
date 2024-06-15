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
                fprintf(stderr, "Error: Too many items in the list\n");
                exit(EXIT_FAILURE);
            }
            // Ensure the string length is within bounds before copying
            if (strlen(line) >= MAX_KEY_LENGTH) {
                fprintf(stderr, "Error: List item too long\n");
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

// Function to print message queue usage based on mode and owner
void print_msgqueue_usage_test(char mode, const char *owner, char key_list[MAX_LIST_ITEMS][MAX_KEY_LENGTH], int key_list_count) {
    FILE *fp;
    char buffer[BUFFER_SIZE];
    char command[] = "ipcs -q";
    char *line;

    // Execute the ipcs -q command and open a pipe to read the output
    if ((fp = popen(command, "r")) == NULL) {
        fprintf(stderr, "Error: Unable to execute command 'ipcs -q'\n");
        exit(EXIT_FAILURE);
    }


    // Read the command output line by line
    LOG(DEB,"Printing Message Queue Usage");
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
                LOG(DEB,"Message Queue Key: %s, Used Bytes: %d, Messages: %d, Owner: %s",
                                    key, bytes, messages, queue_owner);
            }
        }
    }

    // Close the pipe
    if (pclose(fp) == -1) {
        perror("pclose");
        exit(EXIT_FAILURE);
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

void read_config(const char *filename, char *mode, char *owner, int* usage_limit) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        LOG(ERR,"Error opening file");
        exit(EXIT_FAILURE);
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Ignore commented lines
        if (line[0] == '#') {
            continue;
        }

        // Find the position of '='
        char *equals = strchr(line, '=');
        if (equals) {
            *equals = '\0';
            char *key = trim_whitespace(line);
            char *value = trim_whitespace(equals + 1);

            if (strcmp(key, "MODE") == 0) {
                *mode = value[0];  // Store the first character of the value
            } else if (strcmp(key, "OWNER") == 0) {
                strcpy(owner, value);
            } else if (strcmp(key, "LIMIT") == 0)
            {
                *usage_limit = atoi(value);
            }
        }
    }

    fclose(file);
}

int main(int argc, char *argv[]) {
    
    char mode = '\0';
    char owner[50] = "";
    int usage_limit = -1;
    int msgmax, msgmnb, msgmni, max_messages;
    int used_bytes, messages;
    char key_list[MAX_LIST_ITEMS][MAX_KEY_LENGTH];
    int key_list_count;


    // Validate number of arguments
    if (argc != 2) {
        LOG(ERR, "Usage: %s <config_filename>", argv[0]);
        exit(EXIT_FAILURE);
    }
 
    // Validate config file
    read_config(argv[1], &mode, owner, &usage_limit);
    if (mode == '\0')
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

    LOG(DEB,"Execution parameters | Mode: %c | Owner: %s | Limit: %d", mode, owner, usage_limit);

    // Get message queue limits
    get_msg_queue_limits(&msgmax, &msgmnb, &msgmni);

    // Print the values
    LOG(DEB,"OS message queue limits | max msg size (%d) | max queue bytes (%d)", msgmax, msgmnb);

    // Print message queue search
    print_msgqueue_usage_test(mode, owner, key_list, key_list_count);

    return 0;
}

