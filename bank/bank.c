#include "bank.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ports.h"

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
        free(bank);
    }
}

ssize_t bank_send(Bank *bank, char *data, size_t data_len) {
    RSA *key = bank->key;

    int enc_len =
        RSA_public_encrypt(data_len + 1, (unsigned char *)data,
                           (unsigned char *)data, key, RSA_PKCS1_OAEP_PADDING);
    if (enc_len == -1) {
        perror("bank_send encryption error\n");
        ERR_print_errors_fp(stderr);
        return -1;
    }

    // Returns the number of bytes sent; negative on error
    return sendto(bank->sockfd, data, enc_len, 0,
                  (struct sockaddr *)&bank->rtr_addr, sizeof(bank->rtr_addr));
}

ssize_t bank_recv(Bank *bank, char *data, size_t max_data_len) {
    RSA *key = bank->key;

    // Returns the number of bytes received; negative on error
    int data_len = recvfrom(bank->sockfd, data, max_data_len, 0, NULL, NULL);
    if (data_len < 0) return data_len;

    // for (int i = 0; i < data_len; i++) printf("%02X ", (unsigned
    // char)data[i]); printf("\n\n");

    int msg_len =
        RSA_private_decrypt(data_len, (unsigned char *)data,
                            (unsigned char *)data, key, RSA_PKCS1_OAEP_PADDING);
    if (msg_len == -1) {
        perror("bank_recv decryption error\n");
        ERR_print_errors_fp(stderr);
        return -1;
    }

    // for (int i = 0; i < msg_len; i++) printf("%02X ", (unsigned
    // char)data[i]);

    return msg_len;
}

void bank_process_local_command(Bank *bank, char *command, size_t len) {
    // TODO: Implement the bank's local commands
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
