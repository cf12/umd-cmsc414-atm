#include "atm.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int starts_with(const char *a, const char *b) {
    return !strncmp(a, b, strlen(b));
}

ATM *atm_create() {
    ATM *atm = (ATM *)malloc(sizeof(ATM));
    if (atm == NULL) {
        perror("Could not allocate ATM");
        exit(1);
    }

    // Set up the network state
    atm->sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    bzero(&atm->rtr_addr, sizeof(atm->rtr_addr));
    atm->rtr_addr.sin_family = AF_INET;
    atm->rtr_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    atm->rtr_addr.sin_port = htons(ROUTER_PORT);

    bzero(&atm->atm_addr, sizeof(atm->atm_addr));
    atm->atm_addr.sin_family = AF_INET;
    atm->atm_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    atm->atm_addr.sin_port = htons(ATM_PORT);
    bind(atm->sockfd, (struct sockaddr *)&atm->atm_addr, sizeof(atm->atm_addr));

    // Set up the protocol state
    // TODO set up more, as needed

    return atm;
}

void atm_free(ATM *atm) {
    if (atm != NULL) {
        close(atm->sockfd);
        EVP_PKEY_free(atm->key);
        EVP_cleanup();
        free(atm);
    }
}

ssize_t atm_send(ATM *atm, char *data, size_t data_len) {
    // TODO: len(data) < N, lol
    ssize_t out_len = rsa_encrypt(atm->key, (unsigned char *)data, data_len);

    // Returns the number of bytes sent; negative on error
    return sendto(atm->sockfd, data, out_len, 0,
                  (struct sockaddr *)&atm->rtr_addr, sizeof(atm->rtr_addr));
}

ssize_t atm_recv(ATM *atm, char *data, size_t max_data_len) {
    // Returns the number of bytes received; negative on error
    int data_len = recvfrom(atm->sockfd, data, max_data_len, 0, NULL, NULL);
    if (data_len < 0) return data_len;

    ssize_t out_len = rsa_decrypt(atm->key, (unsigned char *)data, data_len);
    return out_len;
}

void process_begin_session_command(ATM *atm, size_t argc, char **argv) {
    if (argc != 2) {
        printf("Usage: begin-session <user-name>\n");
        return;
    }

    char *username = argv[1];

    // fucking shit boy math
    char card_filename[255 + 1] = "";
    strncpy(card_filename, username, 250);
    strcat(card_filename, ".card");
    

    FILE *card_file = fopen(card_filename, "r");
    if (!card_file) {
        printf("Unable to access %s's card\n", username);
        return;
    }

    printf("PIN? ");

    char pin[4];
    if (scanf("%4s", &pin) != 1) {
        printf("Not authorized\n");
        return;
    }

    packet_t p = {
        .cmd = BeginSession,
        .nonce = 1
    };

    memcpy(p.username, username, sizeof(*username));
    memcpy(p.pin, pin, sizeof(*pin));
    memcpy(p.card, "aaaa", 4);

    printf("%d %250s %4s %16s %d\n", p.cmd, p.username, p.pin, p.card, p.nonce);

    char recvline[10000];

    atm_send(atm, (char*) &p, sizeof(p));
    int n = atm_recv(atm, recvline, 10000);
    recvline[n] = 0;
    printf("$%s\n", recvline);

    return;
}

void atm_process_command(ATM *atm, char *command) {
    // TODO: Implement the ATM's side of the ATM-bank protocol

    /*
     * The following is a toy example that simply sends the
     * user's command to the bank, receives a message from the
     * bank, and then prints it to stdout.
     */

    char recvline[10000];
    char sendline[10000];

    size_t argc = 0;
    char **argv = malloc(sizeof(char *));
    char *splitter;

    // TODO: null byte ovbeflow?
    command[strlen(command) - 1] = '\0';

    splitter = strtok(command, " ");
    while (splitter != NULL) {
        char *tmp = malloc(strlen(splitter) + 1);
        strcpy(tmp, splitter);
        argv[argc++] = tmp;
        splitter = strtok(NULL, " ");
    }

    char *cmd = argv[0];
    if (strcmp(cmd, "begin-session") == 0) {
        process_begin_session_command(atm, argc, argv);
    } else if (strcmp(cmd, "withdraw") == 0) {
        if (argc != 2) {
            printf("Usage: withdraw <amt>\n");
        } else {
        }
    } else if (strcmp(cmd, "balance") == 0) {
        if (argc != 1) {
            printf("Usage: balance\n");
        } else {
            strcpy(sendline, "0 brian 0000 card1 balance");
            atm_send(atm, sendline, strlen(sendline));
            int n = atm_recv(atm, recvline, 10000);
            recvline[n] = 0;
            printf("$%s\n", recvline);
        }
    } else {
        printf("Invalid command\n");
    }

    for (int i = 0; i < argc; i++) free(argv[i]);
    free(argv);
}
