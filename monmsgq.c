#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ctype.h>

#define BUFFER_SIZE 1024

void get_msgqueue_usage(const char* key, int* used_bytes, int* messages) {
    FILE *fp;
    char buffer[BUFFER_SIZE];
    char command[] = "ipcs -q";
    char *line;
    int found = 0;

    // Execute the ipcs -q command and open a pipe to read the output
    if ((fp = popen(command, "r")) == NULL) {
        perror("popen");
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
        fprintf(stderr, "No message queue with key %s found.\n", key);
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

int get_value_from_file(const char *filename) {
    FILE *fp;
    int value;

    fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    if (fscanf(fp, "%d", &value) != 1) {
        perror("fscanf");
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


int isStringIntRange(const char *str, int min, int max) {
    // Check if the string is empty
    if (*str == '\0') {
        return 0;
    }

    int number = 0;

    // Check each character in the string
    while (*str != '\0') {
        // Check if the character is a digit
        if (!isdigit(*str)) {
            return 0;
        }

        // Convert the character to an integer
        int digit = *str - '0';

        // Update the number by multiplying by 10 and adding the new digit
        number = number * 10 + digit;

        // Move to the next character
        str++;
    }

    // Check if the integer is within the range of 0 to 99
    if (number < min || number > max) {
        return 0;
    }

    return 1;
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

void read_config(const char *filename, char *mode, char *owner) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
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
            }
        }
    }

    fclose(file);
}

int main(int argc, char *argv[]) {
    
    char mode = '\0';
    char owner[50] = "";
    int msgmax, msgmnb, msgmni, max_messages;
    int used_bytes, messages;

    // Validate number of arguments
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <message_queue_key> <usage_limit 0-99>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Validate usage limit values
    if (!isStringIntRange(argv[2],0, 99))
    {
        fprintf(stderr, "Invalid usage_limit argument. Usage: %s <message_queue_key> <usage_limit 0-99>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Validate config file
    read_config("monmsgq.config", &mode, owner);
    if (mode == '\0')
    {
        fprintf(stderr, "Missing MODE parameter in configuration file.\n");
        exit(EXIT_FAILURE);
    }else if (mode == 'O' && strlen(owner)==0)
    {
        fprintf(stderr, "Missing OWNER parameter in configuration file.\n");
        exit(EXIT_FAILURE);
    }

    printf("Mode: %c\n", mode);
    printf("Owner: %s\n", owner);

    // Get message queue limits
    get_msg_queue_limits(&msgmax, &msgmnb, &msgmni);

    // Print the values
    max_messages = msgmnb / msgmax;
    printf("Maximum size of a single message (msgmax): %d\n", msgmax);
    printf("Maximum number of bytes in all messages on a single queue (msgmnb): %d\n", msgmnb);
    printf("Maximum number of messages: %d\n", max_messages);

    // Print message queue search
    const char *key = argv[1];
    get_msgqueue_usage(key, &used_bytes, &messages);
    if (used_bytes != -1 && messages != -1) {

        int usageLimit = atoi(argv[2]);
        
        float byte_usage = ((float)used_bytes / msgmnb) * 100;

        printf("\nFor message queue %s:\n", key);
        printf("Used bytes: %d (%.2lf%c)\n", used_bytes, byte_usage,'%');
        printf("Limit %d\n", usageLimit);


    }
    return 0;
}

