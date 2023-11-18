/*
 * The main program for the Bank.
 *
 * You are free to change this as necessary.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>

#include "../ports.h"
#include "bank.h"
#include "../rsa/rsa.h"

static const char prompt[] = "BANK: ";

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: bank <init-fname>\n");
        return -1;
    }

    int n;
    char sendline[10000];
    char recvline[10000];

    Bank* bank = bank_create();
    EVP_PKEY* key = rsa_readkey(strcat(argv[1], ".bank"));
    if (!key) {
        perror("Error opening bank initialization file");
        return 64;
    }

    bank->key = key;

    printf("%s", prompt);
    fflush(stdout);

    while (1) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        FD_SET(bank->sockfd, &fds);
        select(bank->sockfd + 1, &fds, NULL, NULL, NULL);

        if (FD_ISSET(0, &fds)) {
            fgets(sendline, 10000, stdin);
            bank_process_local_command(bank, sendline, strlen(sendline));
            printf("%s", prompt);
            fflush(stdout);
        } else if (FD_ISSET(bank->sockfd, &fds)) {
            n = bank_recv(bank, recvline, 10000);
            bank_process_remote_command(bank, recvline, n);
        }
    }

    return EXIT_SUCCESS;
}
