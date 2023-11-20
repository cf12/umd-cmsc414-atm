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
    atm->username = NULL;
    atm->card = 0;
    atm->pin = 0;

    atm->nonce = 0;

    return atm;
}

void atm_free(ATM *atm) {
    if (atm != NULL) {
        close(atm->sockfd);
        EVP_PKEY_free(atm->key);
        EVP_cleanup();

        if (atm->username) free(atm->username);

        free(atm);
    }
}

ssize_t atm_send(ATM *atm, char *data, size_t data_len) {
    // TODO: len(data) < N, lol
    unsigned char *out;
    ssize_t out_len =
        rsa_encrypt(atm->key, (unsigned char *)data, data_len, &out);

    // increment nonce
    atm->nonce += 1;

    // Returns the number of bytes sent; negative on error
    return sendto(atm->sockfd, out, out_len, 0,
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
    } else if (atm->username) {
        printf("A user is already logged in\n");
        return;
    }

    char *username = argv[1];

    packet_t p1 = {.cmd = CheckSession,
                   .username = {0},
                   .card = 0,
                   .pin = 0,
                   .nonce = atm->nonce};

    memcpy(p1.username, username, 250);

    char recvline1[10000];

    atm_send(atm, (char *)&p1, sizeof(p1));
    ssize_t n = atm_recv(atm, recvline1, 10000);
    recvline1[n] = 0;

    // CheckSession failed
    if (n != 0) {
        printf("%s\n", recvline1);
        return;
    }

    // user exists, set up BeginSession
    char card_filename[256] = "";
    strncpy(card_filename, username, 250);
    strcat(card_filename, ".card");
    printf("%s\n", card_filename);

    int card = 0;

    FILE *card_file = fopen(card_filename, "r");
    if (card_file != NULL) {
        // printf("Unable to access %s's card\n", username
        fscanf(card_file, "%d", &card);
    }

    printf("PIN? ");

    char user_input[10000];
    int pin;

    fgets(user_input, 10000, stdin);
    if (sscanf(user_input, "%4u ", &pin) != 1) {
        printf("Not authorized\n");
        return;
    }

    packet_t p2 = {.cmd = BeginSession,
                   .username = {0},
                   .card = card,
                   .pin = pin,
                   .nonce = atm->nonce};

    memcpy(p2.username, username, 250);

    // printf("%d %s %4u %u %d\n", p.cmd, p.username, p.pin, p.card, p.nonce);

    // for (int i = 0; i < sizeof(p); i++)
    //   printf("%02X ", ((char*) &p)[i]);
    // printf("\n");

    char recvline2[10000];

    atm_send(atm, (char *)&p2, sizeof(p2));
    n = atm_recv(atm, recvline2, 10000);
    recvline2[n] = 0;

    if (n == 0) {
        printf("Authorized\n");
        atm->username = malloc(strlen(username) + 1);
        strcpy(atm->username, username);
        atm->pin = pin;
        atm->card = card;
    } else {
        printf("%s\n", recvline2);
    }

    return;
}

void process_withdraw_command(ATM *atm, size_t argc, char **argv) {
    char recvline[10000];
    int amt = 0;

    if (argc != 2 || (sscanf(argv[1], "%d", &amt) != 1) || amt < 0) {
        printf("Usage: withdraw <amt>\n");
        return;
    } else if (!atm->username) {
        printf("No user logged in\n");
        return;
    }

    packet_t p = {.cmd = Withdraw,
                  .username = {0},
                  .card = atm->card,
                  .pin = atm->pin,
                  .nonce = atm->nonce,
                  .amt = amt};
    memcpy(p.username, atm->username, 250);

    atm_send(atm, (char *)&p, sizeof(p));
    int n = atm_recv(atm, recvline, 10000);
    recvline[n] = 0;
    printf("%s\n", recvline);
}

void process_balance_command(ATM *atm, size_t argc, char **argv) {
    char recvline[10000];

    if (argc != 1) {
        printf("Usage: balance\n");
        return;
    } else if (!atm->username) {
        printf("No user logged in\n");
        return;
    }

    packet_t p = {.cmd = Balance,
                  .username = {0},
                  .card = atm->card,
                  .pin = atm->pin,
                  .nonce = atm->nonce,
                  .amt = 0};
    memcpy(p.username, atm->username, 250);

    atm_send(atm, (char *)&p, sizeof(p));
    int n = atm_recv(atm, recvline, 10000);
    recvline[n] = 0;
    printf("%s\n", recvline);
}

void process_end_session_command(ATM *atm, size_t argc, char **argv) {
    char recvline[10000];

    if (!atm->username) {
        printf("No user logged in\n");
        return;
    }

    packet_t p = {.cmd = EndSession,
                  .username = {0},
                  .card = atm->card,
                  .pin = atm->pin,
                  .nonce = atm->nonce,
                  .amt = 0};
    memcpy(p.username, atm->username, 250);

    atm_send(atm, (char *)&p, sizeof(p));
    int n = atm_recv(atm, recvline, 10000);
    recvline[n] = 0;

    if (n == 0) {
        // success
        atm->username = NULL;
        atm->card = 0;
        atm->pin = 0;

        printf("User logged out\n");
    } else {
        printf("%s\n", recvline);
    }
}

void atm_process_command(ATM *atm, char *command) {
    // TODO: Implement the ATM's side of the ATM-bank protocol

    /*
     * The following is a toy example that simply sends the
     * user's command to the bank, receives a message from the
     * bank, and then prints it to stdout.
     */

    size_t argc = 0;
    char **argv = malloc(sizeof(char *));
    char *splitter;

    if (strlen(command) <= 1) return;

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
    if (strcmp(cmd, "begin-session") == 0)
        process_begin_session_command(atm, argc, argv);
    else if (strcmp(cmd, "withdraw") == 0)
        process_withdraw_command(atm, argc, argv);
    else if (strcmp(cmd, "balance") == 0)
        process_balance_command(atm, argc, argv);
    else if (strcmp(cmd, "end-session") == 0)
        process_end_session_command(atm, argc, argv);
    else
        printf("Invalid command\n");

    for (int i = 0; i < argc; i++) free(argv[i]);
    free(argv);
}
