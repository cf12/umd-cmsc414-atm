
#include "bank.h"

#include <math.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <regex.h>

regex_t create_user_regex, deposit_regex, balance_user_regex;

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

    // Set up Hashtable
    HashTable *pt = hash_table_create(10);
    HashTable *bt = hash_table_create(10);
    HashTable *ct = hash_table_create(10);

    bank->pin_table = pt;
    bank->balance_table = bt;
    bank->card_table = ct;

    // Set up user list
    List *cu = list_create();
    bank->users = cu;

    // Set up none
    bank->nonce = 0;
    
    // Set up regex
    regcomp(&create_user_regex,
                   "^create-user ([a-zA-Z]+) ([0-9][0-9][0-9][0-9]) ([0-9]+)\n$",
                   REG_EXTENDED);
    regcomp(&deposit_regex, "^deposit ([a-zA-Z]+) ([0-9]+)\n$", REG_EXTENDED);
    regcomp(&balance_user_regex, "^balance ([a-zA-Z]+)\n$", REG_EXTENDED);

    return bank;
}

void bank_free(Bank *bank) {
    if (bank != NULL) {
        close(bank->sockfd);
        EVP_PKEY_free(bank->key);
        EVP_cleanup();

        hash_table_free(bank->pin_table);
        hash_table_free(bank->balance_table);
        hash_table_free(bank->card_table);

        list_free(bank->users);

        free(bank);
    }
}

ssize_t bank_send(Bank *bank, char *data, size_t data_len) {
    unsigned char *out;
    ssize_t out_len =
        rsa_encrypt(bank->key, (unsigned char *)data, data_len, &out);

    // Returns the number of bytes sent; negative on error
    return sendto(bank->sockfd, out, out_len, 0,
                  (struct sockaddr *)&bank->rtr_addr, sizeof(bank->rtr_addr));
}

ssize_t bank_recv(Bank *bank, char *data, size_t max_data_len) {
    // Returns the number of bytes received; negative on error
    int data_len = recvfrom(bank->sockfd, data, max_data_len, 0, NULL, NULL);
    if (data_len < 0) return data_len;

    ssize_t out_len = rsa_decrypt(bank->key, (unsigned char *)data, data_len);
    return out_len;
}

void process_create_user_command(Bank *bank, char *command) {
    HashTable *bt = bank->balance_table, *pt = bank->pin_table,
              *ct = bank->card_table;
    char *username = calloc(1, 251 * sizeof(*username));
    int *pin = calloc(1, sizeof(*pin));
    int *balance = calloc(1, sizeof(*balance));
    int *card = calloc(1, sizeof(*card));
    int num_groups = 3;
    int num_matched =
        sscanf(command, "create-user %250s %4d %d", username, pin, balance);

    // check input valid
    if (num_matched != num_groups || *balance < 0) {
        free(username);
        free(pin);
        free(balance);
        free(card);
        printf("Usage:  create-user <user-name> <pin> <balance>\n");
        return;
    }

    if (hash_table_find(bt, username) == NULL) {
        FILE *fp;
        char filename[256] = "";
        strcat(filename, username);
        strcat(filename, ".card");

        fp = fopen(filename, "w");

        // File was successfully created
        if (fp != NULL) {
            // TODO: prevent collisions
            *card = rand();
            fprintf(fp, "%d", *card);

            printf("Created user %s\n", username);
            fclose(fp);

            hash_table_add(pt, username, pin);
            hash_table_add(bt, username, balance);
            hash_table_add(ct, username, card);
            return;
        }

        printf("Error creating card file for user %s\n", username);
    } else {
        printf("Error:  user %s already exists\n", username);
    }
}

void process_deposit_command(Bank *bank, char *command) {
    HashTable *bt = bank->balance_table;
    char *user_name = calloc(1, 251 * sizeof(*user_name));
    int amt = 0;
    int num_groups = 2;
    int num_matched = sscanf(command, "deposit %250s %d", user_name, &amt);

    // check input valid
    if (num_matched != num_groups || amt < 0) {
        free(user_name);
        printf("Usage:  deposit <user-name> <amt>\n");
        return;
    }

    // if user_name exists
    if (hash_table_find(bt, user_name) != NULL) {
        int *balance = hash_table_find(bt, user_name);

        if (*balance + amt > 0) {
            *balance += amt;

            printf("$%d added to %s's account\n", amt, user_name);
            return;
        }

        printf("Too rich for this program\n");
    } else {
        printf("No such user\n");
    }
}

void process_balance_command(Bank *bank, char *command) {
    HashTable *bt = bank->balance_table;
    char *user_name = calloc(1, 251 * sizeof(*user_name));
    int num_groups = 1;
    int num_matched = sscanf(command, "balance %250s", user_name);

    // check input valid
    if (num_matched != num_groups) {
        free(user_name);
        printf("Usage:  balance <user-name>\n");
        return;
    }

    int *balance;
    if ((balance = hash_table_find(bt, user_name)) != NULL) {
        printf("$%d\n", *balance);
    } else {
        printf("No such user\n");
    }
}

void bank_process_local_command(Bank *bank, char *command, size_t len) {
    int matched; 

    if (starts_with(command, "create-user")) {
        matched =
            !regexec(&create_user_regex, command, 0, NULL, 0);

        if (matched) {
            process_create_user_command(bank, command);
        } else {
            printf("Usage:  create-user <user-name> <pin> <balance>\n");
        }
    } else if (starts_with(command, "deposit")) {
        matched =
            !regexec(&deposit_regex, command, 0, NULL, 0);

        if (matched) {
            process_deposit_command(bank, command);
        } else {
            printf("Usage:  deposit <user-name> <amt>\n");
        }
    } else if (starts_with(command, "balance")) {
        matched =
            !regexec(&balance_user_regex, command, 0, NULL, 0);

        if (matched) {
            process_balance_command(bank, command);
        } else {
            printf("Usage:  balance <user-name>\n");
        }
    } else {
        printf("Invalid command\n");
    }
}

void bank_process_remote_command(Bank *bank, char *command, size_t len) {
    HashTable *bt = bank->balance_table, *pt = bank->pin_table,
              *ct = bank->card_table;
    List *users = bank->users;
    int *user_pin, *user_card, *user_balance;
    char sendline[10000];
    packet_t *p;

    p = (packet_t *)command;
    user_pin = hash_table_find(pt, p->username);
    user_card = hash_table_find(ct, p->username);
    user_balance = hash_table_find(bt, p->username);

    // printf("[packet]: %d %s %4u %u %d\n", p->cmd, p->username, p->pin, p->card,
    //        p->nonce);
    // if (user_pin != NULL && user_card != NULL && user_balance != NULL)
    //     printf("[user]: %d %d $%d\n", *user_pin, *user_card, *user_balance);

    switch (p->cmd) {
        case CheckSession:
            // user doesn't exist
            if (hash_table_find(bt, p->username) == NULL) {
                strcpy(sendline, "No such user");
            } else if (list_find(users, p->username) != NULL) {
                strcpy(sendline, "A user is already logged in");
            } else {
                strcpy(sendline, "");
            }
            break;
        case BeginSession:
            if (p->pin != *user_pin || p->card != *user_card ||
                p->nonce != bank->nonce) {
                strcpy(sendline, "Not authorized");
            } else {
                strcpy(sendline, "");
            }

            break;
        case Withdraw:
            if (p->pin != *user_pin || p->card != *user_card ||
                p->nonce != bank->nonce) {
                strcpy(sendline, "Not authorized");
            } else if (user_balance == NULL) {
                strcpy(sendline, "No user logged in");
            } else if (p->amt < 0 || *user_balance < p->amt) {
                strcpy(sendline, "Insufficient funds");
            } else {
                *user_balance -= p->amt;
                sprintf(sendline, "$%d dispensed", p->amt);
            }

            break;
        case Balance:
            if (p->pin != *user_pin || p->card != *user_card ||
                p->nonce != bank->nonce) {
                strcpy(sendline, "Not authorized");
            } else if (user_balance == NULL) {
                strcpy(sendline, "No user logged in");
            } else {
                sprintf(sendline, "$%d", *user_balance);
            }

            break;
        case EndSession:
            if (p->pin != *user_pin || p->card != *user_card ||
                p->nonce != bank->nonce) {
                strcpy(sendline, "Not authorized");
            } else if (user_balance == NULL) {
                strcpy(sendline, "No user logged in");
            } else {
                list_del(users, p->username);
                strcpy(sendline, "");
            }

            break;
        default:
            break;
    }

    // respond to atm
    bank_send(bank, sendline, strlen(sendline));

    // increment nonce
    bank->nonce += 1;
}