/*
 * The Bank takes commands from stdin as well as from the ATM.
 *
 * Commands from stdin be handled by bank_process_local_command.
 *
 * Remote commands from the ATM should be handled by
 * bank_process_remote_command.
 *
 * The Bank can read both .card files AND .pin files.
 *
 * Feel free to update the struct and the processing as you desire
 * (though you probably won't need/want to change send/recv).
 */

#ifndef __BANK_H__
#define __BANK_H__

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>

#include "../ports.h"
#include "../util/hash_table.h"
#include "../util/packet.h"
#include "../util/rsa.h"
#include "../util/list.h"

typedef struct _Bank {
    // Networking state
    int sockfd;
    struct sockaddr_in rtr_addr;
    struct sockaddr_in bank_addr;

    // Protocol state
    // TODO add more, as needed


    // HashTable
    HashTable *pin_table;
    HashTable *balance_table;
    HashTable *card_table;

    List *users;

    int nonce;
    EVP_PKEY *key;
} Bank;

Bank *bank_create();
void bank_free(Bank *bank);
ssize_t bank_send(Bank *bank, char *data, size_t data_len);
ssize_t bank_recv(Bank *bank, char *data, size_t max_data_len);
void bank_process_local_command(Bank *bank, char *command, size_t len);
void bank_process_remote_command(Bank *bank, char *command, size_t len);

#endif
