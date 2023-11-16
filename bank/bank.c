
#include "bank.h"

#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ports.h"
#define HANDLE_BANK_ERROR            \
    {                                \
        perror("bank error\n");      \
        ERR_print_errors_fp(stderr); \
        return -1;                   \
    }
;

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
        free(bank);
    }
}

ssize_t bank_send(Bank *bank, unsigned char *data, size_t data_len) {
    size_t out_len;
    EVP_PKEY_CTX *ctx;
    EVP_PKEY *key = bank->key;

    ctx = EVP_PKEY_CTX_new(key, NULL);
    if (!ctx) HANDLE_BANK_ERROR;
    if (EVP_PKEY_encrypt_init(ctx) <= 0) HANDLE_BANK_ERROR;
    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0)
        HANDLE_BANK_ERROR;
    if (EVP_PKEY_encrypt(ctx, NULL, &out_len, data, data_len) <= 0)
        HANDLE_BANK_ERROR;
    if (EVP_PKEY_encrypt(ctx, data, &out_len, data, data_len) <= 0)
        HANDLE_BANK_ERROR;

    // Returns the number of bytes sent; negative on error
    return sendto(bank->sockfd, data, out_len, 0,
                  (struct sockaddr *)&bank->rtr_addr, sizeof(bank->rtr_addr));
}

ssize_t bank_recv(Bank *bank, unsigned char *data, size_t max_data_len) {
    size_t out_len;
    unsigned char *out, *in;
    EVP_PKEY_CTX *ctx;
    EVP_PKEY *key = bank->key;

    // Returns the number of bytes received; negative on error
    int data_len = recvfrom(bank->sockfd, data, max_data_len, 0, NULL, NULL);
    if (data_len < 0) return data_len;

    ctx = EVP_PKEY_CTX_new(key, NULL);
    if (!ctx) HANDLE_BANK_ERROR;
    if (EVP_PKEY_decrypt_init(ctx) <= 0) HANDLE_BANK_ERROR;
    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0)
        HANDLE_BANK_ERROR;
    if (EVP_PKEY_decrypt(ctx, NULL, &out_len, data, data_len) <= 0)
        HANDLE_BANK_ERROR;
    if (EVP_PKEY_decrypt(ctx, data, &out_len, data, data_len) <= 0)
        HANDLE_BANK_ERROR;

    return out_len;
}

void bank_process_local_command(Bank *bank, char *command, size_t len) {
    // TODO: Implement the bank's local commands
    if (starts_with(&command, "create-user")) {
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
    sprintf(sendline, "Bank got: %s", (unsigned char *)command);
    bank_send(bank, (unsigned char *)sendline, strlen(sendline));
    printf("Received the following:\n");
    fputs(command, stdout);
}