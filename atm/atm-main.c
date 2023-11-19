/*
 * The main program for the ATM.
 *
 * You are free to change this as necessary.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../util/rsa.h"
#include "atm.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: atm <init-fname>\n");
        return -1;
    }

    char user_input[10000];

    ATM* atm = atm_create();
    EVP_PKEY* key = rsa_readkey(strcat(argv[1], ".bank"));
    if (!key) {
        perror("Error opening ATM initialization file");
        return 64;
    }

    atm->key = key;

    printf("ATM: ");
    fflush(stdout);

    while (fgets(user_input, 10000, stdin) != NULL) {
        atm_process_command(atm, user_input);
        if (atm->username)
            printf("ATM (%s): ", atm->username);
        else
            printf("ATM: ");
        fflush(stdout);
    }

    return EXIT_SUCCESS;
}
