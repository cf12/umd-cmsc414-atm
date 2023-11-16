/*
 * The main program for the ATM.
 *
 * You are free to change this as necessary.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atm.h"

static const char prompt[] = "ATM: ";

int main(int argc, char** argv) {
    char user_input[1000];

    ATM* atm = atm_create();
    const char* filename = strcat(argv[1], ".bank");

    FILE* keyFile = fopen(filename, "r");
    if (!keyFile) {
        perror("Error opening bank initialization file");
        return 64;
    }

    RSA* key = PEM_read_RSAPrivateKey(keyFile, NULL, NULL, NULL);
    if (!key) {
        perror("Error opening bank initialization file");
        return 64;
    }

    atm->key = key;

    printf("%s", prompt);
    fflush(stdout);

    while (fgets(user_input, 10000, stdin) != NULL) {
        atm_process_command(atm, user_input);
        printf("%s", prompt);
        fflush(stdout);
    }

    fclose(keyFile);
    RSA_free(atm->key);
    EVP_cleanup();

    return EXIT_SUCCESS;
}
