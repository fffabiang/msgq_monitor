/* TEST ONLY: Use to send a message to the msg queue */
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>

// Define message structure
struct msg_buffer {
    long msg_type;
    char msg_text[100];
};

int main(int argc, char *argv[]) {
    key_t key;
    int msgid;
    struct msg_buffer message;

    // Check for correct number of arguments
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <key> <type> <message>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Convert command line arguments
    key = (key_t)strtol(argv[1], NULL, 16); // Key in hexadecimal
    message.msg_type = atol(argv[2]);
    strncpy(message.msg_text, argv[3], sizeof(message.msg_text) - 1);
    message.msg_text[sizeof(message.msg_text) - 1] = '\0';

    // Create message queue
    msgid = msgget(key, 0666 | IPC_CREAT);
    if (msgid == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    // Send message
    if (msgsnd(msgid, &message, sizeof(message.msg_text), 0) == -1) {
        perror("msgsnd");
        exit(EXIT_FAILURE);
    }

    printf("Message sent: %s\n", message.msg_text);

    return 0;
}
