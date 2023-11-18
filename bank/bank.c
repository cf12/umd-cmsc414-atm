
#include "bank.h"

#include <math.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../ports.h"
#include "../rsa/rsa.h"

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
    // TODO set up more, as needed

    // Set up Hashtable
    HashTable *pt = hash_table_create(10);
    HashTable *bt = hash_table_create(10);
    bank->pin_table = pt;
    bank->balance_table = bt;

    return bank;
}

void bank_free(Bank *bank) {
    if (bank != NULL) {
        close(bank->sockfd);
        EVP_PKEY_free(bank->key);
        EVP_cleanup();
        hash_table_free(bank->pin_table);
        hash_table_free(bank->balance_table);
        free(bank);
    }
}

ssize_t bank_send(Bank *bank, char *data, size_t data_len) {
    ssize_t out_len = rsa_encrypt(bank->key, (unsigned char *)data, data_len);

    // Returns the number of bytes sent; negative on error
    return sendto(bank->sockfd, data, out_len, 0,
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
    HashTable *bt = bank->balance_table, *pt = bank->pin_table;
    char *user_name = calloc(1, 251 * sizeof(*user_name));
    int *pin = calloc(1, sizeof(*pin));
    int *balance = calloc(1, sizeof(*balance));
    int num_groups = 3;
    int num_matched =
        sscanf(command, "create-user %250s %4d %d", user_name, pin, balance);

    // check input valid
    if (num_matched != num_groups || *balance < 0) {
        free(user_name);
        free(pin); 
        free(balance);
        printf("Usage:  create-user <user-name> <pin> <balance>\n");
        return;
    }

    if (hash_table_find(bt, user_name) == NULL) {
        FILE *fp;
        char filename[] = "";
        strcat(filename, user_name);
        strcat(filename, ".card");

        fp = fopen(filename, "w");

        // File was successfully created
        if (fp != NULL) {
            // TODO add info to card
            printf("Created user %s\n", user_name);
            fclose(fp);

            hash_table_add(pt, user_name, pin);
            hash_table_add(bt, user_name, balance);
            return;
        }

        printf("Error creating card file for user %s\n", user_name);
    } else {
        printf("Error:  user %s already exists\n", user_name);
    }
}

void process_deposit_command(Bank *bank, char *command) {
    HashTable *bt = bank->balance_table;
    char *user_name = calloc(1, 251 * sizeof(*user_name));
    int *amt = calloc(1, sizeof(*amt));
    int num_groups = 2;
    int num_matched = sscanf(command, "deposit %250s %d", user_name, amt);

    // check input valid
    if (num_matched != num_groups || *amt < 0) {
        free(user_name);
        free(amt);
        printf("Usage:  deposit <user-name> <amt>\n");
        return;
    }

    // if user_name exists
    if (hash_table_find(bt, user_name) != NULL) {
        int *new_balance = malloc(sizeof(*new_balance));
        int *balance = hash_table_find(bt, user_name);
        printf("initial: %d, deposit: %d\n", *balance, *amt);

        *new_balance = *balance + *amt;
        if (new_balance > 0) {
            printf("initial: %d, amt: %d, updated: %d\n", *balance, *amt,
                   *new_balance);

            hash_table_del(bt, user_name);
            hash_table_add(bt, user_name, new_balance);

            printf("$%d added to %s's account\n", *amt, user_name);
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
    if (starts_with(command, "create-user")) {
        process_create_user_command(bank, command);
    } else if (starts_with(command, "deposit")) {
        process_deposit_command(bank, command);
    } else if (starts_with(command, "balance")) {
        process_balance_command(bank, command);
    } else {
        printf("Invalid command\n");
    }
}

typedef enum { BeginSession } Command;
typedef struct {
    Command cmd;
    char username[250];
    char pin[4];
    char card[16];
    uint32_t nonce;
} packet_t;


void bank_process_remote_command(Bank *bank, char *command, size_t len) {
    // TODO: Implement the bank side of the ATM-bank protocol

    for (int i = 0; i < len; i++)
      printf("%02X ", command[i]);
    printf("\n");

    HashTable *ht = bank->hash_table;
    // size_t argc = 0;
    // char **argv = malloc(sizeof(char *));
    // char *splitter;

    packet_t* p = (packet_t*) command;

    printf("%d %s %s %s %d\n", p->cmd, p->username, p->pin, p->card, p->nonce);



    // // TODO: null byte ovbeflow?
    // command[len] = 0;

    // splitter = strtok(command, " ");
    // while (splitter != NULL) {
    //     char *tmp = malloc(strlen(splitter) + 1);
    //     strcpy(tmp, splitter);
    //     argv[argc++] = tmp;
    //     splitter = strtok(NULL, " ");
    // }


    // const int bound = 4;

    // unsigned int nonce = atoi(argv[0]);
    // char *user_name = argv[1];
    // char *pin = argv[2];
    // char *card = argv[3];
    // char *cmd = argv[bound];

    // for (int i = 0; i < argc; i++)
    //   printf("[%d]: %s\n", i, argv[i]);


    // char sendline[10000];
    // if (strcmp(cmd, "login") == 0) {
    //   if (hash_table_find(ht, user_name) != NULL) {

    //   } else {
    //     sprintf(sendline, "No such user", command);
    //   }
    // } else if (strcmp(cmd, "balance") == 0) {
    //   int *balance;
    //   if ((balance = hash_table_find(ht, user_name)) != NULL) {
    //     sprintf(sendline, "%d", *balance);
    //   } 
    // } else {
    //     printf("comm error\n");
    // }

    // bank_send(bank, sendline, strlen(sendline));

    // for (int i = 0; i < argc; i++)
    //     free(argv[i]);
    // // free(argv);
}