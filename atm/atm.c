#include "atm.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ports.h"

#define HANDLE_ATM_ERROR     \
    {                        \
        perror("atm error"); \
        return -1;           \
    };

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
        free(atm);
    }
}

ssize_t atm_send(ATM *atm, unsigned char *data, size_t data_len) {
    // TODO: len(data) < N, lol
    size_t out_len;
    EVP_PKEY_CTX *ctx;
    EVP_PKEY *key = atm->key;

    ctx = EVP_PKEY_CTX_new(key, NULL);
    if (!ctx) HANDLE_ATM_ERROR;
    if (EVP_PKEY_encrypt_init(ctx) <= 0) HANDLE_ATM_ERROR;
    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0)
        HANDLE_ATM_ERROR;
    if (EVP_PKEY_encrypt(ctx, NULL, &out_len, data, data_len) <= 0)
        HANDLE_ATM_ERROR;
    if (EVP_PKEY_encrypt(ctx, data, &out_len, data, data_len) <= 0)
        HANDLE_ATM_ERROR;

    // Returns the number of bytes sent; negative on error
    return sendto(atm->sockfd, data, out_len, 0,
                  (struct sockaddr *)&atm->rtr_addr, sizeof(atm->rtr_addr));
}

ssize_t atm_recv(ATM *atm, unsigned char *data, size_t max_data_len) {
    size_t out_len;
    unsigned char *out, *in;
    EVP_PKEY_CTX *ctx;
    EVP_PKEY *key = atm->key;

    // Returns the number of bytes received; negative on error
    int data_len = recvfrom(atm->sockfd, data, max_data_len, 0, NULL, NULL);
    if (data_len < 0) return data_len;

    ctx = EVP_PKEY_CTX_new(key, NULL);
    if (!ctx) HANDLE_ATM_ERROR;
    if (EVP_PKEY_decrypt_init(ctx) <= 0) HANDLE_ATM_ERROR;
    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0)
        HANDLE_ATM_ERROR;
    if (EVP_PKEY_decrypt(ctx, NULL, &out_len, data, data_len) <= 0)
        HANDLE_ATM_ERROR;
    if (EVP_PKEY_decrypt(ctx, data, &out_len, data, data_len) <= 0)
        HANDLE_ATM_ERROR;

    return out_len;
}

void atm_process_command(ATM *atm, char *command) {
    // TODO: Implement the ATM's side of the ATM-bank protocol

    /*
     * The following is a toy example that simply sends the
     * user's command to the bank, receives a message from the
     * bank, and then prints it to stdout.
     */

    char recvline[10000];
    int n;

    atm_send(atm, (unsigned char *)command, strlen(command));
    n = atm_recv(atm, (unsigned char *)recvline, 10000);
    recvline[n] = 0;
    fputs(recvline, stdout);
}
