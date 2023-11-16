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

#define HANDLE_FILE_ERROR                                 \
    {                                                     \
        perror("Error opening bank initialization file"); \
        return 64;                                        \
    }

int main(int argc, char** argv) {
    EVP_PKEY* key;
    char user_input[1000];

    ATM* atm = atm_create();
    const char* filename = strcat(argv[1], ".bank");

    FILE* key_file = fopen(filename, "r");
    if (!key_file) HANDLE_FILE_ERROR;

    key = PEM_read_PrivateKey(key_file, NULL, NULL, NULL);
    if (!key) HANDLE_FILE_ERROR;

    atm->key = key;

    printf("%s", prompt);
    fflush(stdout);

    while (fgets(user_input, 10000, stdin) != NULL) {
        atm_process_command(atm, user_input);
        printf("%s", prompt);
        fflush(stdout);
    }

    fclose(key_file);
    EVP_PKEY_free(atm->key);
    EVP_cleanup();

    return EXIT_SUCCESS;
}
