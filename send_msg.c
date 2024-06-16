#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>

// Define a larger message structure
#define MSG_TEXT_SIZE 8192 // Assuming this does not exceed the system's MSGMAX limit
struct msg_buffer {
    long msg_type;
    char msg_text[MSG_TEXT_SIZE];
};

int main(int argc, char *argv[]) {
    key_t key;
    int msgid;
    struct msg_buffer message;
    int num_bytes;

    // Check for correct number of arguments
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <key> <num_bytes>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Convert command line arguments
    key = (key_t)strtol(argv[1], NULL, 16); // Key in hexadecimal
    num_bytes = atoi(argv[2]);

    // Validate num_bytes
    if (num_bytes <= 0 || num_bytes > sizeof(message.msg_text)) {
        fprintf(stderr, "Invalid number of bytes. Must be between 1 and %lu.\n", sizeof(message.msg_text));
        exit(EXIT_FAILURE);
    }

    // Set message type and fill message text with whitespaces
    message.msg_type = 1; // Use a default message type
    memset(message.msg_text, ' ', num_bytes);
    message.msg_text[num_bytes] = '\0'; // Null-terminate the message text

    // Create message queue
    msgid = msgget(key, 0666 | IPC_CREAT);
    if (msgid == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    // Send message
    if (msgsnd(msgid, &message, num_bytes, 0) == -1) {
        perror("msgsnd");
        exit(EXIT_FAILURE);
    }

    printf("Number of bytes written: %d\n", num_bytes);

    return 0;
}
