#include "atm.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ports.h"
#include "rsa/rsa.h"

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

void atm_process_command(ATM *atm, char *command) {
    // TODO: Implement the ATM's side of the ATM-bank protocol

    /*
     * The following is a toy example that simply sends the
     * user's command to the bank, receives a message from the
     * bank, and then prints it to stdout.
     */

    char recvline[10000];
    int n;

    atm_send(atm, command, strlen(command));
    n = atm_recv(atm, recvline, 10000);
    recvline[n] = 0;
    fputs(recvline, stdout);
}
