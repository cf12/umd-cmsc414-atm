
#include "bank.h"

#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ports.h"
#include "rsa/rsa.h"

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
    HashTable *ht = hash_table_create(10);
    bank->hash_table = ht; 

    return bank;
}

void bank_free(Bank *bank) {
    if (bank != NULL) {
        close(bank->sockfd);
        EVP_PKEY_free(bank->key);
        EVP_cleanup();
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

char *match_group(char* input_string, regmatch_t *group_array, int i) {
  // index given is not valid
  if (i < 0 || group_array[i].rm_so == (size_t)-1) {
    return NULL;
  }

  // extract match group i from input_string
  int start = group_array[i].rm_so;
  int end = group_array[i].rm_eo; 

  char *matched = malloc((end - start + 1) * sizeof(char));
  if (matched == NULL) {
    perror("Could not allocate string");
    exit(1);
  }

  strncpy(matched, input_string + start, end - start);
  matched[end] = 0;

  return matched;
}

void create_user_command(Bank *bank, char *command, int max_groups,
                         regmatch_t *group_array) {
    HashTable *ht = bank->hash_table;

    // extract match groups
    char *user_name = match_group(command, group_array, 1);
    char *pin = match_group(command, group_array, 2);
    char *balance = match_group(command, group_array, 3);
    
    // if user_name is valid, create a new user
    if (hash_table_find(ht, user_name) == NULL) {
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

        hash_table_add(ht, user_name, balance);
        return;
      }

      printf("Error creating card file for user %s\n", user_name);
    } else {
      printf("Error:  user %s already exists\n", user_name);
    }

    // if any step was invalid, rollback everything
    free(user_name);
    free(pin); 
    free(balance);
}

void deposit_command(Bank *bank, char *command, int max_groups,
                     regmatch_t *group_array) {
    // TODO
}

void balance_command(Bank *bank, char *command, int max_groups,
                     regmatch_t *group_array) {
    // TODO
}

void bank_process_local_command(Bank *bank, char *command, size_t len) {
    // init regex
    int matched, max_groups;
    regex_t create_user_regex, deposit_regex, balance_user_regex;
    regcomp(&create_user_regex,
                   "^create-user ([a-zA-Z]+) ([0-9][0-9][0-9][0-9]) ([0-9]+)\n$",
                   REG_EXTENDED);
    regcomp(&deposit_regex, "^deposit ([a-zA-Z]+) ([0-9]+)\n$", REG_EXTENDED);
    regcomp(&balance_user_regex, "^balance ([a-zA-Z]+)\n$", REG_EXTENDED);

    if (starts_with(command, "create-user")) {
        max_groups = 4;
        regmatch_t group_array[max_groups];
        matched =
            !regexec(&create_user_regex, command, max_groups, group_array, 0);

        if (matched) {
            create_user_command(bank, command, max_groups, group_array);
        } else {
            printf("Usage:  create-user <user-name> <pin> <balance>\n");
        }
    } else if (starts_with(command, "deposit")) {
        // TODO
    } else if (starts_with(command, "balance")) {
        // TODO
    } else {
        printf("Invalid command\n");
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
    sprintf(sendline, "Bank got: %s", command);
    bank_send(bank, sendline, strlen(sendline));
    printf("Received the following:\n");
    fputs(command, stdout);
}