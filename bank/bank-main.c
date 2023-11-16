/*
 * The main program for the Bank.
 *
 * You are free to change this as necessary.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>

#include "bank.h"
#include "../ports.h"

static const char prompt[] = "BANK: ";

#define HANDLE_FILE_ERROR                                 \
    {                                                     \
        perror("Error opening bank initialization file"); \
        return 64;                                        \
    }

int main(int argc, char** argv) {
    EVP_PKEY* key;
    int n;
    char sendline[1000];
    char recvline[1000];

    Bank* bank = bank_create();
    const char* filename = strcat(argv[1], ".bank");

    FILE* key_file = fopen(filename, "r");
    if (!key_file) HANDLE_FILE_ERROR;

    key = PEM_read_PrivateKey(key_file, NULL, NULL, NULL);
    if (!key) HANDLE_FILE_ERROR;

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
            n = bank_recv(bank, (unsigned char*)recvline, 10000);
            bank_process_remote_command(bank, recvline, n);
        }
    }

    fclose(key_file);
    EVP_PKEY_free(bank->key);
    EVP_cleanup();

    return EXIT_SUCCESS;
}
