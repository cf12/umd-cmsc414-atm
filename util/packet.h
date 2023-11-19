#ifndef __PACKET_H__
#define __PACKET_H__

typedef enum { 
    CheckSession,
    BeginSession,
    EndSession,
    Withdraw,
    Balance,
 } Command;
typedef struct {
    Command cmd;
    char username[251];
    int card;
    int pin;
    int nonce;

    // Withdraw
    int amt;
} packet_t;

#endif