
#include "bank.h"

#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../ports.h"
#include "rsa/rsa.h"

int starts_with(const char *a, const char *b) {
    return !strncmp(a, b, strlen(b));
}

Bank *bank_create() {
    Bank *bank = (Bank *)malloc(sizeof(Bank));
    if (bank == NULL) {
        perror("Could not allocate Bank");
        exit(1);
    }

    // Set up the network state
    bank->sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    bzero(&bank->rtr_addr, sizeof(bank->rtr_addr));
    bank->rtr_addr.sin_family = AF_INET;
    bank->rtr_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    bank->rtr_addr.sin_port = htons(ROUTER_PORT);

    bzero(&bank->bank_addr, sizeof(bank->bank_addr));
    bank->bank_addr.sin_family = AF_INET;
    bank->bank_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    bank->bank_addr.sin_port = htons(BANK_PORT);
    bind(bank->sockfd, (struct sockaddr *)&bank->bank_addr,
         sizeof(bank->bank_addr));

    // Set up the protocol state
    // TODO set up more, as needed

    return bank;
}

void bank_free(Bank *bank) {
    if (bank != NULL) {
        close(bank->sockfd);
        EVP_PKEY_free(bank->key);
        EVP_cleanup();
        free(bank);
    }
}

ssize_t bank_send(Bank *bank, char *data, size_t data_len) {
    ssize_t out_len = rsa_encrypt(bank->key, (unsigned char *)data, data_len);

    // Returns the number of bytes sent; negative on error
    return sendto(bank->sockfd, data, out_len, 0,
                  (struct sockaddr *)&bank->rtr_addr, sizeof(bank->rtr_addr));
}

ssize_t bank_recv(Bank *bank, char *data, size_t max_data_len) {
    // Returns the number of bytes received; negative on error
    int data_len = recvfrom(bank->sockfd, data, max_data_len, 0, NULL, NULL);
    if (data_len < 0) return data_len;

    ssize_t out_len = rsa_decrypt(bank->key, (unsigned char *)data, data_len);
    return out_len;
}

void create_user_command(Bank *bank, char *command, int max_groups,
                         regmatch_t *group_array) {
    int i;
    for (i = 0; i < max_groups; i++) {
        if (group_array[i].rm_so == (size_t)-1) break;  // No more groups

        char sourceCopy[strlen(command) + 1];
        strcpy(sourceCopy, command);
        sourceCopy[group_array[i].rm_eo] = 0;
        printf("Group %u: [%2u-%2u]: %s\n", i, group_array[i].rm_so,
               group_array[i].rm_eo, sourceCopy + group_array[i].rm_so);
    }
}

void deposit_command(Bank *bank, char *command, int max_groups,
                     regmatch_t *group_array) {
    // TODO
}

void balance_command(Bank *bank, char *command, int max_groups,
                     regmatch_t *group_array) {
    // TODO
}

void bank_process_local_command(Bank *bank, char *command, size_t len) {
    // init regex
    int reti, matched, max_groups;
    regex_t create_user_regex, deposit_regex, balance_user_regex;
    reti = regcomp(&create_user_regex,
                   "^create-user [a-zA-Z]+ [0-9][0-9][0-9][0-9] [0-9]+$",
                   REG_EXTENDED);
    reti = regcomp(&deposit_regex, "^deposit [a-zA-Z]+ [0-9]+$", REG_EXTENDED);
    reti = regcomp(&balance_user_regex, "^balance [a-zA-Z]+$", REG_EXTENDED);

    if (starts_with(command, "create-user")) {
        max_groups = 4;
        regmatch_t group_array[max_groups];
        matched =
            !regexec(&create_user_regex, command, max_groups, group_array, 0);

        if (matched) {
            create_user_command(bank, command, max_groups, group_array);
        } else {
            printf("Usage:  %s", command);
        }
    } else if (starts_with(command, "deposit")) {
        // TODO
    } else if (starts_with(command, "balance")) {
        // TODO
    } else {
        printf("Invalid command");
    }
}

void bank_process_remote_command(Bank *bank, char *command, size_t len) {
    // TODO: Implement the bank side of the ATM-bank protocol

    /*
     * The following is a toy example that simply receives a
     * string from the ATM, prepends "Bank got: " and echoes
     * it back to the ATM before printing it to stdout.
     */

    char sendline[10000];
    command[len] = 0;
    sprintf(sendline, "Bank got: %s", command);
    bank_send(bank, sendline, strlen(sendline));
    printf("Received the following:\n");
    fputs(command, stdout);
}